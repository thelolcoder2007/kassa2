# ama-ma35d.nix
#
# Consolidated NixOS module for the AMD Alveo MA35D media accelerator,
# combining everything worked out so far:
#
#   - Kernel module: built from vendored DKMS source (Verisilicon/Hantro
#     VPU driver layout) rather than a prebuilt .ko — see "kernel module"
#     section below for the ARCH_TYPE fix this needed.
#   - Userspace (ffmpeg + libs): repackaged from the AMA SDK .deb files
#     with autoPatchelfHook, with a workaround for a patchelf PT_NOTE
#     bug that hit the prebuilt binaries.
#   - System-level bits: IOMMU/hugepages per AMD's sizing formula, udev
#     device permissions, and a first-boot reminder for the manual
#     firmware-flash/validate steps AMD's docs require.
#
# STILL A SCAFFOLD IN PLACES. Anything marked TODO hasn't been verified
# against the real files yet — fill in as you go.
#
# USAGE:
#   1. Kernel module source: extract
#      /opt/amd/ama/ma35/module/kmod.tar.gz into
#      ./vendor/ama-transcoder-src in this repo and git-commit it as-is.
#   2. Userspace: get the amd-ama-*.deb packages via AMD's apt/dnf repo
#      (see AMA SDK docs "Package Feed Setup") — `apt download
#      amd-ama-driver amd-ama-core amd-ama-xma amd-ama-firmware
#      amd-ama-ffmpeg` on a scratch Debian/Ubuntu box — and put them in
#      one directory.
#   3. imports = [ ./ama-ma35d.nix ];
#      hardware.ama-ma35d = {
#        enable = true;
#        debDir = /root/ama-debs;
#        kmodSrc = ./vendor/ama-transcoder-src;
#        # kmodPatches = [ ./patches/ama-transcoder-kernel-fix.patch ];
#      };
#   4. nixos-rebuild switch, reboot, then run the one-time flash/validate
#      steps printed at activation.

{
  config,
  lib,
  pkgs,
  ...
}:

# with lib;

let
  cfg = config.hardware.ama-ma35d;
  sdkVersion = "1.5.0";

  # ======================================================================
  # KERNEL MODULE — built from vendored DKMS source
  # ======================================================================
  #
  # This is a standard Verisilicon/Hantro VPU driver layout: multiple
  # hardware blocks (vc8000d.c decode, vc8000e.c encode, xabr_scaler.c,
  # xav1_enc.c AV1, riscv.c co-processor, vcmd/ command-queue files) all
  # gated behind SUB_SYS_*/VCMD_ENABLE_* make variables, whose per-card
  # defaults live in config_evb (?= assignments, so they only apply when
  # not already set — command-line vars always win over them).
  #
  # config_evb here targets SOC_PLATFORM=amd-supernova (this card's
  # internal codename) and its defaults are trusted as-is EXCEPT
  # ARCH_TYPE, which config_evb sets to "arm" (this SDK's baseline
  # target is an embedded ARM eval board) — that sends the kernel build
  # system looking for arch/arm/Makefile even on an x86_64 PCIe host, so
  # it must be forced.
  amaKernelModule = config.boot.kernelPackages.callPackage (
    { stdenv, kernel }:
    stdenv.mkDerivation {
      pname = "ama-transcoder";
      version = sdkVersion;

      src = cfg.kmodSrc;
      patches = cfg.kmodPatches;

      nativeBuildInputs = kernel.moduleBuildDependencies;

      # Matches the real Makefile: it wants KERNEL_DIR (not KDIR) and
      # its `all` target internally delegates to Kbuild with
      # M=`pwd` AQROOT=`pwd`. MODULE_NAME defaults to ama_transcoder,
      # so the .ko this produces has a known, fixed name.
      buildPhase = ''
        runHook preBuild
        make \
          KERNEL_DIR=${kernel.dev}/lib/modules/${kernel.modDirVersion}/build \
          ARCH_TYPE=x86_64 \
          ${lib.concatStringsSep " " cfg.kmodExtraMakeFlags} \
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
        description = "AMD Alveo MA35D ama_transcoder kernel module (vendored DKMS source, AMA SDK ${sdkVersion})";
        platforms = lib.platforms.linux;
      };
    }
  ) { };

  # ======================================================================
  # USERSPACE — AMA SDK .deb packages repackaged as prebuilt binaries
  # ======================================================================

  amaUnpacked = pkgs.stdenv.mkDerivation {
    pname = "ama-sdk-unpacked";
    version = sdkVersion;
    src = cfg.debDir;

    nativeBuildInputs = [ pkgs.dpkg ];
    dontUnpack = true;
    dontBuild = true;

    installPhase = ''
      mkdir -p $out
      for deb in $src/amd-ama-*.deb; do
        echo "Unpacking $deb"
        dpkg-deb -x "$deb" $out
      done
    '';
  };

  amaUserspace = pkgs.stdenv.mkDerivation {
    pname = "ama-sdk-userspace";
    version = sdkVersion;
    src = amaUnpacked;

    # Libraries the prebuilt binaries almost certainly link against.
    # TODO: run `patchelf --print-needed` / `ldd` on the real binaries
    # once unpacked and extend this list until autoPatchelfHook stops
    # complaining about missing dependencies.
    buildInputs = with pkgs; [
      # keep-sorted start
      alsa-lib
      aprutil
      boost
      fontconfig
      freetype
      fribidi
      glib
      harfbuzz
      libbsd
      libdrm
      libhugetlbfs
      libkrb5
      libpulseaudio
      libxcursor
      libxext
      libxi
      libxinerama
      libxkbcommon
      libxrandr
      libxxf86vm
      numactl
      openssl
      stdenv.cc.cc.lib # libstdc++
      systemd
      wayland
      xorg.libXScrnSaver
      zlib
      # keep-sorted end
    ];

    nativeBuildInputs = with pkgs; [
      autoPatchelfHook
      makeWrapper
      binutils
      file
    ];

    dontBuild = true;
    dontConfigure = true;

    installPhase = ''
      mkdir -p $out
      cp -r $src/opt $out/opt

      # autoPatchelfHook walks $out and patches every ELF file it finds
      # automatically as part of the fixup phase — nothing else needed
      # here beyond making sure buildInputs above covers all .so deps.
    '';

    # Work around a long-standing patchelf bug (NixOS/patchelf#400,
    # #255, #217 — still present as of 0.18) where binaries with
    # multiple SHT_NOTE sections that aren't laid out contiguously/
    # aligned the way patchelf expects cause:
    #   "cannot normalize PT_NOTE segment: non-contiguous SHT_NOTE sections"
    # No patchelf version reliably avoids this; the fix is to strip the
    # note sections before autoPatchelfHook's postFixup hook runs.
    # PT_NOTE is just metadata (build-id, ABI tag) — removing it doesn't
    # affect runtime behavior, so this is safe for prebuilt binaries.
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

    # Wrap the ffmpeg binary so LD_LIBRARY_PATH / any env vars their
    # setup.sh would normally export are set without needing to source
    # a shell script first.
    # TODO: confirm real path — this assumes amd-ama-ffmpeg installs to
    # /opt/amd/ama/ma35/bin/ffmpeg. Adjust after inspecting the tree.
    postFixup = ''
      if [ -f "$out/opt/amd/ama/ma35/bin/ffmpeg" ]; then
        wrapProgram "$out/opt/amd/ama/ma35/bin/ffmpeg" \
          --prefix LD_LIBRARY_PATH : "$out/opt/amd/ama/ma35/lib" \
          --prefix PATH : "$out/opt/amd/ama/ma35/bin"
      else
        echo "WARNING: expected ffmpeg binary not found at the guessed" >&2
        echo "path — fix postFixup in ama-ma35d.nix once you've seen" >&2
        echo "the real unpacked tree." >&2
      fi
    '';
  };

  amaFFmpeg = pkgs.writeShellScriptBin "ama-ffmpeg" ''
    exec ${amaUserspace}/opt/amd/ama/ma35/bin/ffmpeg "$@"
  '';

in
{
  options.hardware.ama-ma35d = {
    enable = lib.mkEnableOption "AMD Alveo MA35D media accelerator support (AMA SDK ${sdkVersion})";

    debDir = lib.mkOption {
      type = lib.types.path;
      description = ''
        Local directory containing the amd-ama-*.deb packages, obtained
        via AMD's apt/dnf repo (see AMA SDK docs "Package Feed Setup")
        and pulled down without installing, e.g. `apt download
        amd-ama-driver amd-ama-core amd-ama-xma amd-ama-firmware
        amd-ama-ffmpeg` on a scratch Debian/Ubuntu box.
      '';
      example = "/root/ama-debs";
    };

    kmodSrc = lib.mkOption {
      type = lib.types.path;
      description = ''
        Path to the vendored ama_transcoder DKMS source tree (extracted
        from /opt/amd/ama/ma35/module/kmod.tar.gz and committed into
        this repo).
      '';
      example = "./vendor/ama-transcoder-src";
    };

    kmodPatches = lib.mkOption {
      type = lib.types.listOf lib.types.path;
      default = [ ];
      description = ''
        Patches applied on top of kmodSrc before building — e.g. fixes
        needed to build against your specific kernel version. Generate
        with `git diff > patches/foo.patch` inside the vendored source
        tree after making local edits, then revert the edit so the
        patch is what actually gets applied at build time.
      '';
      example = [ ./patches/ama-transcoder-kernel-fix.patch ];
    };

    kmodExtraMakeFlags = lib.mkOption {
      type = lib.types.listOf lib.types.str;
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

    hugepages = lib.mkOption {
      type = lib.types.int;
      default = 4192;
      description = "2MB hugepages to reserve, per AMD's sizing formula (2048 per device + 96).";
    };
  };

  config = lib.mkIf cfg.enable {

    assertions = [
      {
        assertion = cfg.debDir != null;
        message = "hardware.ama-ma35d.debDir must point at a directory of the amd-ama-*.deb files";
      }
      {
        assertion = cfg.kmodSrc != null;
        message = "hardware.ama-ma35d.kmodSrc must point at the vendored ama_transcoder DKMS source tree";
      }
    ];

    # --- system-level requirements, per AMD's chassis setup docs -------
    boot.kernelParams = [
      "amd_iommu=on" # swap for "intel_iommu=on" on Intel hosts
      "iommu=pt"
    ];
    boot.kernel.sysctl."vm.nr_hugepages" = cfg.hugepages;

    # --- kernel module ---------------------------------------------------
    boot.extraModulePackages = [ amaKernelModule ];
    boot.kernelModules = [ "ama_transcoder" ];

    # --- device permissions ------------------------------------------------
    # TODO: confirm real device node name once the module actually loads —
    # check `ls /dev/` and `dmesg` after insmod.
    users.groups.ama = { };
    services.udev.extraRules = ''
      KERNEL=="ama_transcoder*", GROUP="ama", MODE="0660"
    '';

    # --- userspace / ffmpeg -----------------------------------------------
    environment.systemPackages = [ amaFFmpeg ];

    system.activationScripts.ama-ma35d-reminder = ''
      cat <<'EOF'

      [ama-ma35d] One-time manual steps after first boot:
        1. sudo lspci -vvvd 10ee:                         # card visible?
        2. lsmod | grep ama_transcoder                    # module loaded?
        3. sudo /opt/amd/ama/ma35/bin/mamgmt flash -d all \
             -p /opt/amd/ama/ma35/firmware/ma35_firmware.bin
        4. sudo systemctl poweroff                        # cold reboot after flash
        5. cat /sys/class/misc/ama_transcoder0/version_information
        6. mautil validate -d all

      Add your user to the "ama" group and re-login before using
      ama-ffmpeg without sudo.
      EOF
    '';
  };
}
