{
  config,
  lib,
  pkgs,
  ...
}:
let
  cfg = config.hardware.ama-ma35d;
  inherit (pkgs) stdenv fetchurl;
  inherit (lib)
    concatStringsSep
    platforms
    mkEnableOption
    mkOption
    types
    mkIf
    ;
  version = "1.5.0";
  unpackedDebs = stdenv.mkDerivation (_finalAttrs: {
    pname = "amd-ama-drivers-unpacked";
    inherit version;
    srcs = [
      (fetchurl {
        url = "https://packages.xilinx.com/artifactory/debian-packages/pool/amd-ama-xma_1.5.0-20260424092403.x86_64.deb";
        hash = "sha256-VmUmos7SAcZqUJb9VPIRMKdeW/uTX2FX9vDYzubmPsA=";
      })
      (fetchurl {
        url = "https://packages.xilinx.com/artifactory/debian-packages/pool/amd-ama-core_1.5.0-20260424092403.x86_64.deb";
        hash = "sha256-TmOKJpuxeiQacKLJBzb69bcHSNCsrbbS0Z2hZRmDxU0=";
      })
      (fetchurl {
        url = "https://packages.xilinx.com/artifactory/debian-packages/pool/amd-ama-driver_1.5.0-20260424092403.x86_64.deb";
        hash = "sha256-sK/eQzeJeWZq3e+CvRG/t4ENnc1lUZTZOiJqzA9bch4=";
      })
      (fetchurl {
        url = "https://packages.xilinx.com/artifactory/debian-packages/pool/amd-ama-firmware_1.5.0-20260424092403.x86_64.deb";
        hash = "sha256-Qb0dn6mPx7TSN3S36W/K96jPkdG8m+GzUYiVdbX8Vu4=";
      })
      (fetchurl {
        url = "https://packages.xilinx.com/artifactory/debian-packages/pool/amd-ama-ffmpeg_1.5.0-20260424092403.x86_64.deb";
        hash = "sha256-NkM8M9wR5mt60/bv/Ig+8BC79/bMC7Vg4PGzkb39EFU=";
      })
    ];

    nativeBuildInputs = [ pkgs.dpkg ];
    dontUnpack = true;
    dontBuild = true;

    installPhase = ''
      mkdir -p $out
      for deb in $srcs; do
        echo "Unpacking $deb"
        dpkg-deb -x "$deb" $out
      done
    '';
  });
  amaUserspace = pkgs.stdenv.mkDerivation {
    pname = "ama-sdk-userspace";
    inherit version;
    src = unpackedDebs;

    nativeBuildInputs = with pkgs; [
      autoPatchelfHook
      makeWrapper
      binutils
      file
      patchelf
    ];

    preInstall = ''
      	sed -i 's/\/bin\/bash/\/usr\/bin\/env bash/g' $(find -type f -name "*.sh")
        # chmod +x $out/opt/amd/ama/ma35/scripts/.on_transcoder_insert.sh
    '';

    buildInputs = with pkgs; [
      stdenv.cc.cc.lib
      zlib
      openssl
      libhugetlbfs
      boost
      numactl
      glib
      wayland
      libxext
      libxcursor
      libxinerama
      libxi
      libxrandr
      apr
      aprutil
      systemd
      alsa-lib
      libbsd
      krb5
      libdrm
      harfbuzz
      freetype
      libpulseaudio
      fribidi
      fontconfig
      libxscrnsaver
      libxkbcommon
      libxxf86vm
    ];

    dontBuild = true;
    dontConfigure = true;

    installPhase = ''
      	runHook preInstall

        mkdir -p $out
        cp -r $src/* $out

        runHook postInstall
    '';

    preFixup = ''
      find "$out" -type f | while read -r f; do
        if ${pkgs.file}/bin/file "$f" | grep -q ELF; then
          ${pkgs.binutils}/bin/objcopy \
            --remove-section=.note.ABI-tag \
            --remove-section=.note.gnu.build-id \
            --remove-section=.note.gnu.property \
            --remove-section=.note.gnu.gold-version \
            "$f" 2>/dev/null || true
        fi
      done
    '';

    postFixup = ''
      if [ -f "$out/opt/amd/ama/ma35/bin/ffmpeg" ]; then
        patchelf --set-rpath '$ORIGIN/../lib' "$out/opt/amd/ama/ma35/bin/ffmpeg"
        wrapProgram "$out/opt/amd/ama/ma35/bin/ffmpeg" \
          --prefix PATH : "$out/opt/amd/ama/ma35/bin"
      else
        echo "WARNING: expected ffmpeg binary not found" >&2
      fi
    '';

  };
  amaFFmpeg = pkgs.writeShellScriptBin "ama-ffmpeg" ''
    exec ${amaUserspace}/opt/amd/ama/ma35/bin/ffmpeg "$@"
  '';

  amaKernelModule = config.boot.kernelPackages.callPackage (
    { stdenv, kernel }:
    stdenv.mkDerivation (_finalAttrs: {
      pname = "ama-transcoder";
      inherit version;

      src = "${unpackedDebs}/opt/amd/ama/ma35/module/kmod.tar.gz";

      sourceRoot = "dkms_source_tree";

      nativeBuildInputs = kernel.moduleBuildDependencies;

      buildPhase = ''
        runHook preBuild
        make \
          KERNEL_DIR=${kernel.dev}/lib/modules/${kernel.modDirVersion}/build \
          ARCH_TYPE=x86_64 \
          ${concatStringsSep " " cfg.kmodExtraMakeFlags} \
          all
        runHook postBuild
      '';

      installPhase = ''
        runHook preInstall
        mkdir -p $out/lib/modules/${kernel.modDirVersion}/extra
        cp ama_transcoder.ko $out/lib/modules/${kernel.modDirVersion}/extra/
        runHook postInstall
      '';

      meta = {
        description = "AMD Alveo MA35D ama_transcoder kernel module (vendored DKMS source, AMA SDK 1.5.0)";
        platforms = platforms.linux;
      };
    })
  ) { };
in
{

  options.hardware.ama-ma35d = {
    enable = mkEnableOption "AMD Alveo MA35D media accelerator support (prebuilt AMA SDK binaries)";

    debDir = mkOption {
      type = types.path;
      description = ''
        Local directory containing the amd-ama-*.deb packages, obtained
        via AMD's apt/dnf repo (see AMA SDK docs "Package Feed Setup")
        and pulled down without installing, e.g. `apt download
        amd-ama-driver amd-ama-core amd-ama-xma amd-ama-firmware
        amd-ama-ffmpeg` on a scratch Debian/Ubuntu box.
      '';
      example = "/root/ama-debs";
    };
    hugepages = mkOption {
      type = types.int;
      default = 4192;
      description = "2MB hugepages to reserve, per AMD's sizing formula (2048 per device + 96).";
    };

    ffmpegBinary = mkOption {
      default = amaFFmpeg;
    };

    kmodExtraMakeFlags = mkOption {
      type = types.listOf types.str;
      default = [ ];
      description = ''
        Extra VAR=value make flags appended to the kernel module build,
        beyond KERNEL_DIR and ARCH_TYPE (which are always set). Left
        empty by default so that config_evb's own SUB_SYS_*/
        VCMD_ENABLE_*/USE_*_CONFIG defaults for this card
        (SOC_PLATFORM=amd-supernova) take effect unmodified — only add
        something here once you've confirmed config_evb's defaults are
        actually wrong for your deployment, e.g. ["DEBUG=1"] for a
        debug build.
      '';
      example = [ "DEBUG=1" ];
    };
  };

  config = mkIf cfg.enable {

    assertions = [
      {
        assertion = cfg.debDir != null;
        message = "hardware.ama-ma35d.debDir must point at a directory of the amd-ama-*.deb files";
      }
    ];

    boot.kernelParams = [
      "amd_iommu=on" # swap for "intel_iommu=on" on Intel hosts
      "iommu=pt"
    ];

    boot.kernel.sysctl."vm.nr_hugepages" = cfg.hugepages;

    boot.extraModulePackages = [ amaKernelModule ];
    boot.kernelModules = [ "ama_transcoder" ];

    users.groups.ama = { };
    services.udev.extraRules = ''
      ACTION=="add", KERNEL=="ama_transcoder", SUBSYSTEM=="module", RUN+="${amaUserspace}/opt/amd/ama/ma35/scripts/.on_transcoder_insert.sh"
    '';

    environment.systemPackages = [ amaFFmpeg ];
  };
}
