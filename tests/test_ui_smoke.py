from pathlib import Path

from streamlit.testing.v1 import AppTest


ROOT = Path(__file__).resolve().parents[1]


def test_streamlit_pages_render_without_exceptions():
    at = AppTest.from_file(ROOT / "app.py", default_timeout=10).run()
    assert not at.exception

    for page in [
        "pages/1_资料分析.py",
        "pages/2_外部数据库检索.py",
        "pages/3_AI智能分析.py",
    ]:
        at.switch_page(page).run(timeout=10)
        assert not at.exception, f"Streamlit page failed to render: {page}"
