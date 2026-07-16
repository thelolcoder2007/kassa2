{
  config,
  lib,
  pkgs,
  ...
}:
{
  imports = [
    ../pkgs/mistserver/options.nix
    ../base/sops.nix
  ];
  sops.secrets.rtmp_key = {
    restartUnits = [ "gstreamer-stream.service" ];
  };

  networking.firewall.extraInputRules = ''
    ip6 saddr { 2a07:54c1:4932::/48, 2001:67c:2564::/48 } tcp dport 4242 accept
    ip saddr { 130.89.0.0/16, 145.126.0.0/16, 87.208.97.61 } tcp dport 4242 accept
  '';

  services.mistServer = {
    enable = true;
    openFirewall = true;
  };
  systemd.services."gstreamer-stream" = {
    after = [
      "network.target"
      "mistserver.service"
    ];
    description = "Stream /dev/video0 to RTMP";
    wantedBy = [ "multi-user.target" ];
    serviceConfig = {
      Environment = "GST_PLUGIN_SYSTEM_PATH_1_0=\"${pkgs.gst_all_1.gstreamer.out}/lib/gstreamer-1.0:${pkgs.gst_all_1.gst-plugins-base}/lib/gstreamer-1.0:${pkgs.gst_all_1.gst-plugins-good}/lib/gstreamer-1.0:${pkgs.gst_all_1.gst-plugins-bad}/lib/gstreamer-1.0:${pkgs.gst_all_1.gst-plugins-ugly}/lib/gstreamer-1.0\"";
      Type = "simple";
      Restart = "always";
      RestartSec = 2;
      TasksMax = "infinity";
      TimeoutStopSec = 8;
      ExecStart = "${lib.getExe pkgs.bash} -c \"${pkgs.gst_all_1.gstreamer}/bin/gst-launch-1.0 v4l2src device=/dev/video0 ! videoconvert ! x264enc ! flvmux ! rtmpsink location=rtmp://127.0.0.1/live/$(${pkgs.coreutils-full}/bin/cat ${
        config.sops.secrets."rtmp_key".path
      })\"";
      User = "root";
      Group = "root";

    };
    path =
      with pkgs.gst_all_1;
      [
        # keep-sorted start
        gst-libav
        gst-plugins-bad
        gst-plugins-base
        gst-plugins-good
        gst-plugins-ugly
        gstreamer
        # keep-sorted end
      ]
      ++ [
        pkgs.v4l2-relayd
      ];
  };
}
