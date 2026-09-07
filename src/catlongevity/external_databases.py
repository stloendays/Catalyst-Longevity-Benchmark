"""External database connectors used by the catalyst intelligence workspace.

The connectors deliberately return normalized dictionaries instead of leaking
provider-specific response shapes into the UI. Network failures are surfaced as
clear exceptions so the application can fail gracefully.
"""
from __future__ import annotations

import json
from typing import Any
from urllib.parse import quote, urlencode
from urllib.request import Request, urlopen


USER_AGENT = "Catalyst-Longevity-Analyzer/1.0 (+https://github.com/stloendays/Catalyst-Longevity-Benchmark)"


def _http_json(url: str, *, data: bytes | None = None, timeout: float = 15.0) -> dict[str, Any]:
    request = Request(
        url,
        data=data,
        headers={
            "User-Agent": USER_AGENT,
            "Accept": "application/json",
            "Content-Type": "application/x-www-form-urlencoded",
        },
        method="POST" if data is not None else "GET",
    )
    with urlopen(request, timeout=timeout) as response:
        return json.loads(response.read().decode("utf-8"))


def normalize_doi(value: str) -> str:
    text = value.strip()
    for prefix in ("https://doi.org/", "http://doi.org/", "doi:", "DOI:"):
        if text.startswith(prefix):
            text = text[len(prefix):]
            break
    return text.strip()


def lookup_crossref_doi(doi: str, *, mailto: str | None = None) -> dict[str, Any]:
    """Retrieve normalized bibliographic metadata for one DOI from Crossref."""
    normalized = normalize_doi(doi)
    if not normalized or "/" not in normalized:
        raise ValueError("DOI 格式不完整")
    url = f"https://api.crossref.org/works/{quote(normalized, safe='')}"
    if mailto:
        url += "?" + urlencode({"mailto": mailto})
    payload = _http_json(url)
    message = payload.get("message", {})
    authors = []
    for author in message.get("author", []) or []:
        name = " ".join(part for part in [author.get("given", ""), author.get("family", "")] if part).strip()
        if name:
            authors.append(name)
    published = message.get("published-print") or message.get("published-online") or message.get("issued") or {}
    date_parts = (published.get("date-parts") or [[]])[0]
    return {
        "source": "Crossref",
        "doi": message.get("DOI", normalized),
        "title": (message.get("title") or [""])[0],
        "journal": (message.get("container-title") or [""])[0],
        "authors": authors,
        "year": date_parts[0] if date_parts else None,
        "publisher": message.get("publisher"),
        "type": message.get("type"),
        "abstract": message.get("abstract"),
        "url": message.get("URL") or f"https://doi.org/{normalized}",
        "license": [item.get("URL") for item in (message.get("license") or []) if item.get("URL")],
        "reference_count": message.get("reference-count"),
        "is_referenced_by_count": message.get("is-referenced-by-count"),
    }


def search_crossref(query: str, *, rows: int = 8, mailto: str | None = None) -> list[dict[str, Any]]:
    """Search Crossref by a free-text bibliographic query."""
    params: dict[str, Any] = {"query.bibliographic": query.strip(), "rows": max(1, min(rows, 20))}
    if mailto:
        params["mailto"] = mailto
    payload = _http_json("https://api.crossref.org/works?" + urlencode(params))
    results = []
    for item in payload.get("message", {}).get("items", []) or []:
        authors = []
        for author in item.get("author", []) or []:
            name = " ".join(part for part in [author.get("given", ""), author.get("family", "")] if part).strip()
            if name:
                authors.append(name)
        date_parts = ((item.get("published-print") or item.get("published-online") or item.get("issued") or {}).get("date-parts") or [[]])[0]
        results.append(
            {
                "source": "Crossref",
                "doi": item.get("DOI"),
                "title": (item.get("title") or [""])[0],
                "journal": (item.get("container-title") or [""])[0],
                "authors": authors,
                "year": date_parts[0] if date_parts else None,
                "publisher": item.get("publisher"),
                "url": item.get("URL"),
                "score": item.get("score"),
            }
        )
    return results


def search_catalysis_hub(*, reactants: str = "", products: str = "", first: int = 10) -> list[dict[str, Any]]:
    """Search Catalysis-Hub reaction-energy records through its GraphQL API."""
    arguments = [f"first:{max(1, min(first, 25))}"]
    if reactants.strip():
        arguments.append(f'reactants:"{reactants.strip()}"')
    if products.strip():
        arguments.append(f'products:"{products.strip()}"')
    query = """{
      reactions(%s) {
        edges {
          node {
            id
            reactants
            products
            chemicalComposition
            reactionEnergy
            activationEnergy
          }
        }
      }
    }""" % ", ".join(arguments)
    payload = _http_json(
        "https://api.catalysis-hub.org/graphql",
        data=urlencode({"query": query}).encode("utf-8"),
    )
    if payload.get("errors"):
        raise RuntimeError(str(payload["errors"]))
    edges = payload.get("data", {}).get("reactions", {}).get("edges", []) or []
    return [
        {
            "source": "Catalysis-Hub",
            "id": edge.get("node", {}).get("id"),
            "reactants": edge.get("node", {}).get("reactants"),
            "products": edge.get("node", {}).get("products"),
            "chemical_composition": edge.get("node", {}).get("chemicalComposition"),
            "reaction_energy_eV": edge.get("node", {}).get("reactionEnergy"),
            "activation_energy_eV": edge.get("node", {}).get("activationEnergy"),
        }
        for edge in edges
    ]
