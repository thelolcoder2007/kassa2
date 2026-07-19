{
  stdenv,
  fetchFromGitHub,
  meson,
  ninja,
  cacert,
  cjson,
  cmake,
  git,
  libmicrohttpd,
  librist,
  mbedtls,
  openssl,
  pkg-config,
  srt,
  srtp,
  usrsctp,
  ...
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "mistserver";
  version = "3.11.1";
  rel = "1";
  src = fetchFromGitHub {
    owner = "ddvtech";
    repo = "mistserver";
    tag = finalAttrs.version;
    hash = "sha256-wghZp8nNUNAH1gpATZ61JNmk0BrDFPAQqBReM6k/gNY=";

    nativeBuildInputs = [
      cacert
      git
      meson
    ];

    postFetch = ''
      (
        cd "$out"
        for prj in subprojects/*.wrap; do
          meson subprojects download "$(basename "$prj" .wrap)"
          rm -rf subprojects/$(basename "$prj" .wrap)/.git
        done
        find subprojects -type d -name .git -prune -execdir rm -r {} +
      )
    '';
  };
  nativeBuildInputs = [
    meson
    ninja
    cmake
    git
    pkg-config
  ];

  buildInputs = [
    cjson
    libmicrohttpd
    librist
    mbedtls
    openssl
    srt
    srtp
    usrsctp
  ];
})
