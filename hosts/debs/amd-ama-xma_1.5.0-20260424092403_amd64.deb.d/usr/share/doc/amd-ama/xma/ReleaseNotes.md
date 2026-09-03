# AMD MA35D SDK v1.5.0 Release Notes

## Overview
+ Welcome to the AMD Media Acceleration Software Development Kit! This software release supports the MA35D AMD Video Acceleration card. These Release Notes serve to enumerate the latest features, limitations, and known issues for this release of the software development kit (SDK) v1.5.0.

+ For general information regarding the features and proper usage of the SDK, refer to the online documentation at https://amd.github.io/ama-sdk/v1.5.0.

## Features in This Release
+ Support for video transcode using either our supplied FFmpeg (n6.1.1), GStreamer (v1.22), or custom Xilinx Media Acceleration (XMA) applications
+ Support for up to 8 x 4Kp60 8-bit or 10-bit video decode of either AVC, HEVC, or VP9 per device
+ Support for up to 4 x 4Kp60 8-bit or 10-bit video decode of AV1 per device
+ Support for up to 2 x 4Kp60 8-bit or 10-bit video transcode of AVC or HEVC per device
+ Support for up to 4 x 4Kp60 8-bit or 10-bit video transcode of AV1 per device
+ Support for up to 1 x 8Kp30 8-bit or 10-bit video transcode of AV1 per device
+ Support for integration with software-based video image filters using our hardware DMA support to/from device memory (FFmpeg or GStreamer)
+ Optional use of our supplied resource management daemon (xrmd) for automated accelerator selection and load balancing
+ Video accelerator utilization reporting when using our supplied resource management daemon (xrmd)
+ Utility applications enabling collection of power, voltage, temperature, and memory for each device as well as aggregate values for the MA35D accelerator card
+ Support for virtualization using either Ubuntu 20.04 or 22.04 in KVM mode with a guest OS also running either Ubuntu 20.04 or 22.04.
+ Support for color space conversion, input rotation, color subsampling, and overlay and tiling using the 2D gpu FFmpeg plugins
+ The 2D gpu and ML engine with ROI model support are available fully available features using FFmpeg
+ ML engine support now available in XMA (without roi_scale xma filter).
+ Face and/or text-based machine learning models for enhanced low resolution video encoding for fine text and/or faces
+ Dual core AV1 encoding enabling 4kp120 and 8Kp30
+ Low latency encoding support (lookahead depths of approximately 1-11 frames)
+ 'fast' preset for AVC, HEVC and AV1 (type 2) encoding for enhanced throughput and/or density
+ HDR10, HDR10+ and/or HLG metadata pass through transcode support
+ New "Content Adaptive Bit Rate" rate control mode for enhanced visual quality and bitrate efficiency
+ Support for encoder levels >= 6
+ Support for new encoder features in our resource management toolchain
+ Support for up to 160 Mbps encode throughput per device
+ New latency_ms command-line parameter to, optionally, specify the encoder lookahead_depth in units of time instead of frames
+ Telemetry is available for all accelerators by using mautil examine -d all -r utilization
+ The mautil validate subcommand supports all encoder codecs and performs PCIe and MMIO tests
+ New ML based content adaptive (mlcae) rate control mode added.
## New in This Release
+ Support for an additional 4 x 4Kp60 8-bit or 10-bit decode of either AVC, HEVC, or VP9 per device
+ Support for FFmpeg v6.1.1 (v5.1.2 deprecated)
+ Compositor 2D gpu plugin support
+ Support for downscaling using the decoder
+ Support for pipelines spanning multiple devices
+ Created hwupload_ama and hwdownload_ama filters to improve DMA throughput.
+ Support for 2D gpu color space conversion, input rotation, color subsampling, and overlay and tiling in GStreamer
+ Support for decoding JPEG images
+ Support for lossy JPEG still image encoding
+ Support for lossless JPEG still image encoding
+ Support for AVIF still image encoding in ffmpeg
+ Dual core AV1 encoding, enabling 4kp120 and 8Kp30, is now supported for all lookahead depths
+ Support for min QP and CABR with ULL
+ Support for configuring CABR VQ offset using -cabr command-line option
+ Support for passing through CEA 608, 708 closed captions in transcode flows
+ Support for passing through video full range flag in transcode flows
+ Support for passing through DolbyVision T.35s in transcode flows
+ Simplified command-line options for configuring CRF
+ Made range used to configure encoder QPs consistent across all codecs
+ Support for Fedora compatibility
+ Support for pipeline and inference_period in ML plugin (either of it can be used in cli currently)
+ Added mlcae rate control mode for 1920x1080p H.264/AVC content adaptive encoding

## Known Limitations
+ This software is compatible with Ubuntu 20.04, Ubuntu 22.04, with kernel version 5.15.0, 5.19, 6.2 or 6.5, Fedora 40, with kernel 6.8, and Debian 12, with kernel 6.1.
+ Host servers must support a BAR4 PCIe memory region of 512 MB
+ A device that has been assigned to a VM cannot share its resources with its host
+ Only one VF per device is supported
+ For maximum server scalability, ensure that a single VF is assigned to a VM
+ For encoding with AV1 type 2, only resolutions that are divisible by 8 are supported
+ For encoding with all other codecs, only resolutions that are divisble by 4 are supported
+ For decoding, only resolutions that are divisible by 2 are supported
+ This version of the SDK does not support splitting a video acceleration use case across multiple MA35D devices
+ 4Kp120 and 8Kp30 for AV1 (type 1) encoder are only supported when using both encoder cores (-cores=2).
+ The multicore option (-cores=2) requires a minimum dimension of width or height of 720 pixels.
+ Using an encoder lookahead_depth value of <11 may limit the number of b-frames or other encoder features such as rate control modes, aq modes and so forth. Refer to the online documentation for further details.
+ When using the encoder 'fast' preset, lookahead_depth must be > 0.
+ Mixing 2-slice AV1 Type 1 encoding and 1-slice encoding is not supported on a single device
+ Encoder does not support dynamic QP maps for lookahead_depth = 0 nor AV1 Type 2 encoding.
+ ML engine is not supported in GStreamer
+ ML engine does not support pipeline and inference_period together (internally disables pipeline when using inference_period).
+ Mlcae rate control mode only works for h264_ama encoder and 1080p resolution (portrait and landscape)
+ roi_scale plugin for ML ROI use case is not available in XMA
+ XRM is experiencing difficulty in fully utilizing resources in a multi-device scenario when used together with the launcher and jobslot_reservation applications.
+ ma35_roi_transcoder_app may show some delay in processing with h264/hevc encoders.
+ DolbyVision RPUs are not currently passed through in transcode flows

## Known Issues
+ A secondary bus reset (SBR) will cause the server to hang. A cold boot of the server will restore function. Installation of our driver package will disable secondary bus reset.
+ AV1 decoding using the included ama_av1dec GStreamer plugin requires the use of an IVF packaged AV1 raw video stream and a corresponding ivfparse plugin to be utilized to demux the AV1 video stream.
+ ROI Model currently doesn't support portrait mode in a specific resolution of 1080x1920 for text model.
+ High density (with high DDR usages) use cases involving ml_ama plugin may not meet 60 fps overall performance at ffmpeg level.
+ Firmware downgrade to GA1.0 may not work on some desktop machines or low end servers. Warm boot the server to get to a working state.
+ System instability may occur when PF device is used while a VF device exists. PF and VF usage should not be mixed.
+ AVIF files currently only support still images (i.e. a single frame) and not image sequences with multiple frames.
+ Overlay with alpha mapping is only supported for up to 1080p resolutions.
