# pre_build_cert.py — generate a dummy https_server.crt assembly file
# that the esp_https_server managed component expects during pioarduino
# source builds.  HuginnESP only uses plain HTTP, so the cert is unused.

import os

Import("env")

build_dir = env.subst("$BUILD_DIR")
cert_s = os.path.join(build_dir, "https_server.crt.S")

os.makedirs(build_dir, exist_ok=True)

if not os.path.exists(cert_s):
    # Minimal PEM-shaped dummy (never used at runtime)
    dummy = b"-----BEGIN CERTIFICATE-----\ndummy\n-----END CERTIFICATE-----\n"
    lines = [
        "/* Auto-generated dummy cert – HuginnESP does not use HTTPS */",
        ".section .rodata.embedded",
        ".align  4",
        ".global _binary_https_server_crt_start",
        ".type   _binary_https_server_crt_start, @object",
        "_binary_https_server_crt_start:",
    ]
    # Inline the bytes so we don't need .incbin paths
    for i in range(0, len(dummy), 16):
        chunk = dummy[i : i + 16]
        lines.append(".byte " + ",".join(str(b) for b in chunk))
    lines.append(".byte 0")  # null terminator (EMBED_TXTFILES convention)
    lines.append(".global _binary_https_server_crt_end")
    lines.append("_binary_https_server_crt_end:")
    lines.append("")

    with open(cert_s, "w") as f:
        f.write("\n".join(lines))
    print(f"  [pre_build_cert] wrote {cert_s}")
