{ pkgs }:

{
  http2 = true;
  http3 = true;
  addSSL = true;
  sslCertificate = "${pkgs.path}/nixos/tests/common/acme/server/acme.test.cert.pem";
  sslCertificateKey = "${pkgs.path}/nixos/tests/common/acme/server/acme.test.key.pem";
}
