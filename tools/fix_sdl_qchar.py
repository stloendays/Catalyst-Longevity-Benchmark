from pathlib import Path

path = Path("native/qt/src/integrationgateway.cpp")
text = path.read_text(encoding="utf-8")
text = text.replace("QChar('-')", "QLatin1Char('-')")
path.write_text(text, encoding="utf-8")
