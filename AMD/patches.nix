# AMD bullshit
{
  imports = [
    ./ama-ma35d.nix
  ];

  hardware.ama-ma35d.enable = true;

  # programs.nix-ld = {
  #   enable = true;
  #   libraries = with pkgs; [
  #     # Core System Libraries
  #     glibc
  #     libgcc
  #     libffi
  #     zlib
  #     openssl
  #     binutils

  #     # AMD SDK / FFmpeg Specific Dependencies
  #     apr
  #     aprutil
  #     boost
  #     glib
  #     numactl
  #     systemd
  #     libbsd
  #     krb5
  #     libssh2
  #     libxml2

  #     # Graphics & Media (Crucial for AMA/FFmpeg)
  #     libdrm
  #     wayland
  #     libxext
  #     libxcursor
  #     libxinerama
  #     libxi
  #     libxrandr
  #     libxkbcommon
  #     libxxf86vm
  #     libpulseaudio
  #     alsa-lib
  #     harfbuzz
  #     freetype
  #     fribidi
  #     fontconfig
  #     libxscrnsaver

  #     # Hugepages & Hardware
  #     libhugetlbfs
  #   ];
  # };

}
