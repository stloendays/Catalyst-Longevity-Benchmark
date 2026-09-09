from pathlib import Path

path = Path("native/qt/src/main.cpp")
text = path.read_text(encoding="utf-8")

include_anchor = '#include "evidencepacket.h"\n'
include_insert = '#include "evidencepacket.h"\n#include "integrationgateway.h"\n'
if '#include "integrationgateway.h"' not in text:
    if include_anchor not in text:
        raise SystemExit("include anchor not found")
    text = text.replace(include_anchor, include_insert, 1)

self_test_anchor = '''        const auto referenceMatches = catalyst::ReferenceKnowledgeBase::matchExperimentContext(builtInDatasets.front().records);\n        if (referenceMatches.isEmpty() || referenceMatches.front().relevanceScore < 45) return 33;\n\n'''
self_test_insert = '''        const auto referenceMatches = catalyst::ReferenceKnowledgeBase::matchExperimentContext(builtInDatasets.front().records);\n        if (referenceMatches.isEmpty() || referenceMatches.front().relevanceScore < 45) return 33;\n\n        catalyst::IntegrationGateway selfTestGateway;\n        catalyst::IntegrationGatewayConfig selfTestGatewayConfig;\n        selfTestGatewayConfig.bindAddress = QStringLiteral("127.0.0.1");\n        selfTestGatewayConfig.controlPort = 0;\n        selfTestGatewayConfig.eventPort = 0;\n        selfTestGatewayConfig.instrumentPort = 0;\n        selfTestGatewayConfig.accessToken = QStringLiteral("self-test-token-0123456789abcdef");\n        QString gatewayTestMessage;\n        if (!selfTestGateway.start(selfTestGatewayConfig, &gatewayTestMessage)) return 35;\n        if (!selfTestGateway.isRunning()\n            || selfTestGateway.controlPort() == 0\n            || selfTestGateway.eventPort() == 0\n            || selfTestGateway.instrumentPort() == 0\n            || selfTestGateway.accessToken() != selfTestGatewayConfig.accessToken) return 36;\n        const auto gatewayCapabilities = selfTestGateway.capabilities();\n        if (gatewayCapabilities.value(QStringLiteral("safety")).toObject()\n                .value(QStringLiteral("hardware_actuation")).toBool(true)) return 37;\n        selfTestGateway.stop();\n        if (selfTestGateway.isRunning()) return 38;\n\n'''
if 'catalyst::IntegrationGateway selfTestGateway;' not in text:
    if self_test_anchor not in text:
        raise SystemExit("self-test anchor not found")
    text = text.replace(self_test_anchor, self_test_insert, 1)

startup_anchor = '''    catalyst::MainWindow window;\n    applyWindowPolish(window);\n    window.show();\n    return app.exec();\n'''
startup_insert = '''    catalyst::IntegrationGateway integrationGateway;\n    const auto integrationConfig = catalyst::IntegrationGateway::configFromArguments(app.arguments());\n    QString integrationMessage;\n    const bool integrationStarted = !integrationConfig.enabled\n        || integrationGateway.start(integrationConfig, &integrationMessage);\n\n    catalyst::MainWindow window;\n    applyWindowPolish(window);\n    if (!integrationConfig.enabled) {\n        window.statusBar()->showMessage(QStringLiteral("自驱动实验室接口已禁用。"), 8000);\n    } else if (!integrationStarted) {\n        window.statusBar()->showMessage(QStringLiteral("自驱动实验室接口未启动：%1").arg(integrationMessage), 15000);\n    } else {\n        window.statusBar()->showMessage(integrationMessage, 8000);\n    }\n    window.show();\n    const int exitCode = app.exec();\n    integrationGateway.stop();\n    return exitCode;\n'''
if 'const auto integrationConfig = catalyst::IntegrationGateway::configFromArguments(app.arguments());' not in text:
    if startup_anchor not in text:
        raise SystemExit("startup anchor not found")
    text = text.replace(startup_anchor, startup_insert, 1)

path.write_text(text, encoding="utf-8")
