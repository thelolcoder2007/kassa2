{ pkgs, ... }:

{
  environment.systemPackages =
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
}
