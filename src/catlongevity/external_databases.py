"""External database connectors for the catalyst intelligence workspace.

All connectors return normalized dictionaries so provider-specific response
formats do not leak into the analysis layer. API keys are accepted only as
runtime arguments or environment variables and are never persisted here.
"""
from __future__ import annotations

import json
import os
from typing import Any
from urllib.parse import quote, urlencode
from urllib.request import Request, urlopen


USER_AGENT = "Catalyst-Longevity-Analyzer (+https://github.com/stloendays/Catalyst-Longevity-Benchmark)"


def _http_json(
    url: str,
    *,
    data: bytes | None = None,
    timeout: float = 20.0,
    extra_headers: dict[str, str] | None = None,
) -> dict[str, Any]:
    headers = {
        "User-Agent": USER_AGENT,
        "Accept": "application/json",
        "Content-Type": "application/x-www-form-urlencoded",
    }
    if extra_headers:
        headers.update(extra_headers)
    request = Request(
        url,
        data=data,
        headers=headers,
        method="POST" if data is not None else "GET",
    )
    with urlopen(request, timeout=timeout) as response:
        return json.loads(response.read().decode("utf-8"))


def normalize_doi(value: str) -> str:
    text = value.strip()
    lowered = text.casefold()
    for prefix in ("https://doi.org/", "http://doi.org/", "doi:"):
        if lowered.startswith(prefix.casefold()):
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


def _semantic_scholar_headers(api_key: str | None = None) -> dict[str, str]:
    key = (api_key or os.getenv("SEMANTIC_SCHOLAR_API_KEY") or "").strip()
    return {"x-api-key": key} if key else {}


def search_semantic_scholar(query: str, *, limit: int = 8, api_key: str | None = None) -> list[dict[str, Any]]:
    """Search Semantic Scholar Academic Graph for related literature."""
    if not query.strip():
        return []
    fields = "paperId,title,year,authors,venue,abstract,citationCount,referenceCount,externalIds,url,openAccessPdf"
    params = {"query": query.strip(), "limit": max(1, min(limit, 20)), "fields": fields}
    payload = _http_json(
        "https://api.semanticscholar.org/graph/v1/paper/search?" + urlencode(params),
        extra_headers=_semantic_scholar_headers(api_key),
    )
    return [_normalize_semantic_scholar_paper(item) for item in payload.get("data", []) or []]


def lookup_semantic_scholar_doi(doi: str, *, api_key: str | None = None) -> dict[str, Any]:
    """Retrieve one Semantic Scholar paper by DOI."""
    normalized = normalize_doi(doi)
    if not normalized or "/" not in normalized:
        raise ValueError("DOI 格式不完整")
    fields = "paperId,title,year,authors,venue,abstract,citationCount,referenceCount,externalIds,url,openAccessPdf"
    identifier = quote(f"DOI:{normalized}", safe=":")
    payload = _http_json(
        f"https://api.semanticscholar.org/graph/v1/paper/{identifier}?" + urlencode({"fields": fields}),
        extra_headers=_semantic_scholar_headers(api_key),
    )
    return _normalize_semantic_scholar_paper(payload)


def _normalize_semantic_scholar_paper(item: dict[str, Any]) -> dict[str, Any]:
    external_ids = item.get("externalIds") or {}
    open_pdf = item.get("openAccessPdf") or {}
    return {
        "source": "Semantic Scholar",
        "paper_id": item.get("paperId"),
        "doi": external_ids.get("DOI"),
        "title": item.get("title"),
        "year": item.get("year"),
        "venue": item.get("venue"),
        "authors": [author.get("name") for author in (item.get("authors") or []) if author.get("name")],
        "abstract": item.get("abstract"),
        "citation_count": item.get("citationCount"),
        "reference_count": item.get("referenceCount"),
        "url": item.get("url"),
        "open_access_pdf": open_pdf.get("url"),
        "external_ids": external_ids,
    }


def lookup_pubchem_compound(name: str) -> dict[str, Any]:
    """Resolve a compound name through PubChem PUG REST."""
    query = name.strip()
    if not query:
        raise ValueError("请输入化合物名称")
    properties = "Title,MolecularFormula,MolecularWeight,CanonicalSMILES,IsomericSMILES,InChI,InChIKey"
    url = (
        "https://pubchem.ncbi.nlm.nih.gov/rest/pug/compound/name/"
        + quote(query, safe="")
        + f"/property/{properties}/JSON"
    )
    payload = _http_json(url)
    rows = payload.get("PropertyTable", {}).get("Properties", []) or []
    if not rows:
        raise LookupError(f"PubChem 未找到化合物：{query}")
    item = rows[0]
    return {
        "source": "PubChem",
        "query": query,
        "cid": item.get("CID"),
        "title": item.get("Title"),
        "molecular_formula": item.get("MolecularFormula"),
        "molecular_weight": item.get("MolecularWeight"),
        "canonical_smiles": item.get("ConnectivitySMILES") or item.get("CanonicalSMILES"),
        "isomeric_smiles": item.get("SMILES") or item.get("IsomericSMILES"),
        "inchi": item.get("InChI"),
        "inchikey": item.get("InChIKey"),
        "url": f"https://pubchem.ncbi.nlm.nih.gov/compound/{item.get('CID')}" if item.get("CID") else None,
    }


def search_materials_project_formula(
    formula: str,
    *,
    api_key: str | None = None,
    limit: int = 10,
) -> list[dict[str, Any]]:
    """Search Materials Project summary records by chemical formula.

    Materials Project requires an API key. The official ``mp-api`` Python client
    is loaded lazily so users who do not use this integration do not need it.
    """
    query = formula.strip()
    if not query:
        return []
    key = (api_key or os.getenv("MP_API_KEY") or "").strip()
    if not key:
        raise ValueError("Materials Project 需要 API Key；请在界面临时输入或设置 MP_API_KEY 环境变量。")
    try:
        from mp_api.client import MPRester
    except ImportError as exc:
        raise RuntimeError("未安装 Materials Project 官方客户端。请运行：pip install mp-api") from exc

    fields = [
        "material_id",
        "formula_pretty",
        "energy_above_hull",
        "band_gap",
        "is_stable",
        "density",
        "volume",
        "symmetry",
    ]
    with MPRester(key) as mpr:
        docs = mpr.materials.summary.search(formula=query, fields=fields)

    results: list[dict[str, Any]] = []
    for doc in list(docs)[: max(1, min(limit, 25))]:
        symmetry = getattr(doc, "symmetry", None)
        results.append(
            {
                "source": "Materials Project",
                "material_id": str(getattr(doc, "material_id", "")) or None,
                "formula": getattr(doc, "formula_pretty", None),
                "is_stable": getattr(doc, "is_stable", None),
                "energy_above_hull_eV_atom": getattr(doc, "energy_above_hull", None),
                "band_gap_eV": getattr(doc, "band_gap", None),
                "density_g_cm3": getattr(doc, "density", None),
                "volume_A3": getattr(doc, "volume", None),
                "crystal_system": getattr(symmetry, "crystal_system", None) if symmetry else None,
                "space_group": getattr(symmetry, "symbol", None) if symmetry else None,
                "url": (
                    f"https://next-gen.materialsproject.org/materials/{getattr(doc, 'material_id', '')}"
                    if getattr(doc, "material_id", None)
                    else None
                ),
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
