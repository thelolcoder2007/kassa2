import os
import gi
import time

gi.require_version('Gst', '1.0')
from gi.repository import Gst, GLib

# Initialize GStreamer
Gst.init(None)


rtmp_key = "recv-tcp"

pipeline_str = (
    "v4l2src device=/dev/video0 io-mode=mmap do-timestamp=true "
    "! capsfilter caps=video/x-raw,width=3840,height=2160,framerate=60/1 ! tee name=t "

    # RTSP Branch
    "t. ! queue max-size-bytes=0 max-size-buffers=60 max-size-time=0 leaky=downstream "
    "! videoconvert ! x264enc bitrate=100000 speed-preset=ultrafast tune=zerolatency key-int-max=60 "
    f"! h264parse config-interval=-1 ! rtspclientsink protocols=tcp location=\"rtsp://127.0.0.1:5554/{rtmp_key}\" "

    # MKV Recording Branch
    "t. ! queue max-size-bytes=0 max-size-buffers=60 max-size-time=0 leaky=downstream "
    "! videoconvert ! x264enc pass=quant quantizer=0 speed-preset=ultrafast tune=zerolatency "
    f"! matroskamux ! filesink location=/var/mistserver/recordings/sntpings-{time.strftime('%Y-%m-%d_%H-%M-%S', time.localtime())}.mkv async=false "

    # PNG encoding branch
    "t. ! queue max-size-bytes=0 max-size-buffers=60 max-size-time=0 leaky=downstream "
    "! videorate drop-only=true ! video/x-raw,framerate=1/1 "
    "! videoconvert ! pngenc "
    "! appsink name=snapsink emit-signals=true sync=false async=false"
)

pipeline = Gst.parse_launch(pipeline_str)

appsink = pipeline.get_by_name("snapsink")

base_dir = "/var/mistserver/screenshots"

def on_new_frame(sink):
    sample = sink.emit("pull-sample")
    if not sample:
        return Gst.FlowReturn.ERROR

    buffer = sample.get_buffer()
    success, map_info = buffer.map(Gst.MapFlags.READ)

    if success:
        now = time.localtime()
        dir_path = time.strftime(f"{base_dir}/%Y-%m-%d-%H", now)
        file_name = time.strftime("%M_%S.png", now)
        full_path = os.path.join(dir_path, file_name)

        os.makedirs(dir_path, exist_ok=True)

        with open(full_path, "wb") as f:
            f.write(map_info.data)

        buffer.unmap(map_info)
        print(f"Snapshot saved: {full_path}")

    return Gst.FlowReturn.OK

bus = pipeline.get_bus()
bus.add_signal_watch()

def on_bus_message(bus, message):
    if message.type == Gst.MessageType.ERROR:
        err, debug = message.parse_error()
        print(f"\n[PIPELINE ERROR]: {err}")
        print(f"[DEBUG INFO]: {debug}\n")
        loop.quit()
    elif message.type == Gst.MessageType.WARNING:
        err, debug = message.parse_warning()
        print(f"[PIPELINE WARNING]: {err}")
    return True

bus.connect("message", on_bus_message)

appsink.connect("new-sample", on_new_frame)

print("Press Ctrl+C to stop.")
pipeline.set_state(Gst.State.PLAYING)

# Run the main loop so the script stays alive
loop = GLib.MainLoop()
try:
    loop.run()
except KeyboardInterrupt:
    print("\nStopping recording...")
finally:
    # Crucial: Send an EOS (End of Stream) so the MKV file finalizes properly without corruption
    pipeline.send_event(Gst.Event.new_eos())
    time.sleep(1)
    pipeline.set_state(Gst.State.NULL)
