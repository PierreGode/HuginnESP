# pre_build_cert.py — generate dummy certificate assembly files that
# managed components (esp_https_server, esp_rainmaker, etc.) expect
# during pioarduino source builds.  HuginnESP does not use any of
# these cloud/TLS services, so the certs are never used at runtime.

import os

Import("env")

build_dir = env.subst("$BUILD_DIR")
os.makedirs(build_dir, exist_ok=True)

# Every .crt name that managed components try to embed via EMBED_TXTFILES.
# The symbol names are derived from the filename: dots and slashes become
# underscores, prefixed with _binary_ and suffixed with _start / _end.
DUMMY_CERTS = [
    "https_server.crt",
    "rmaker_mqtt_server.crt",
    "rmaker_ota_server.crt",
    "rmaker_claim_service_server.crt",
]

dummy_pem = b"-----BEGIN CERTIFICATE-----\ndummy\n-----END CERTIFICATE-----\n"

for cert_name in DUMMY_CERTS:
    cert_s = os.path.join(build_dir, cert_name + ".S")
    if os.path.exists(cert_s):
        continue

    # Symbol name: replace non-alnum with underscore
    sym = cert_name.replace(".", "_").replace("/", "_").replace("-", "_")

    lines = [
        f"/* Auto-generated dummy {cert_name} – not used by HuginnESP */",
        ".section .rodata.embedded",
        ".align  4",
        f".global _binary_{sym}_start",
        f".type   _binary_{sym}_start, @object",
        f"_binary_{sym}_start:",
    ]
    for i in range(0, len(dummy_pem), 16):
        chunk = dummy_pem[i : i + 16]
        lines.append(".byte " + ",".join(str(b) for b in chunk))
    lines.append(".byte 0")  # null terminator (EMBED_TXTFILES convention)
    lines.append(f".global _binary_{sym}_end")
    lines.append(f"_binary_{sym}_end:")
    lines.append("")

    with open(cert_s, "w") as f:
        f.write("\n".join(lines))
    print(f"  [pre_build_cert] wrote {cert_s}")
