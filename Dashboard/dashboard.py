import serial
import json
import matplotlib.pyplot as plt
import numpy as np

ser = serial.Serial('COM3', 921600)
ser.reset_input_buffer()

plt.style.use('seaborn-v0_8')

fig = plt.figure(figsize=(12, 8))
fig.patch.set_facecolor('#f5f7fa')  

# Layout
ax1 = plt.subplot(4,1,1)
ax2 = plt.subplot(4,1,2)
ax3 = plt.subplot(4,1,3)
ax4 = plt.subplot(4,1,4)


RAW_COLOR = '#4c72b0'     # blue
FILT_COLOR = '#55a868'    # green
FFT_COLOR = '#c44e52'     # red
RMS_COLOR = '#dd8452'     # orange
MDF_COLOR = '#8172b2'     # purple

# Lines
raw_line, = ax1.plot([], [], color=RAW_COLOR, linewidth=1.5)
filt_line, = ax2.plot([], [], color=FILT_COLOR, linewidth=1.5)
fft_line, = ax3.plot([], [], color=FFT_COLOR, linewidth=1.5)

rms_line, = ax4.plot([], [], label="RMS", color=RMS_COLOR, linewidth=2)
mdf_line, = ax4.plot([], [], label="MDF", color=MDF_COLOR, linewidth=2)

# Titles
ax1.set_title("Raw EMG Signal", fontsize=11, weight='bold')
ax2.set_title("Filtered EMG Signal", fontsize=11, weight='bold')
ax3.set_title("Frequency Spectrum", fontsize=11, weight='bold')
ax4.set_title("RMS & MDF Trend", fontsize=11, weight='bold')

ax4.legend(loc="upper right")

# Clean axes
for ax in [ax1, ax2, ax3, ax4]:
    ax.grid(True, alpha=0.3)

# History buffers
rms_hist = []
mdf_hist = []

frame_count = 0

# Status panel
status_box = fig.text(
    0.80, 0.80, "",
    fontsize=12,
    bbox=dict(facecolor='white', edgecolor='#333', boxstyle='round,pad=0.5')
)

plt.tight_layout(rect=[0,0,0.78,1])

# ===== LOOP =====
while True:
    try:
        line = ser.readline().decode().strip()
        data = json.loads(line)

        frame_count += 1
        if frame_count < 20:
            continue

        raw = np.array(data["raw"])
        filt = np.array(data["filt"])
        fft = np.array(data["fft"])
        rms = data["rms"]
        mdf = data["mdf"]
        fatigue = data["fatigue"]

        raw = raw - np.mean(raw)

        # Update plots
        raw_line.set_data(np.arange(len(raw)), raw)
        filt_line.set_data(np.arange(len(filt)), filt)
        fft_line.set_data(np.arange(len(fft)), fft)

        rms_hist.append(rms)
        mdf_hist.append(mdf)

        if len(rms_hist) > 100:
            rms_hist.pop(0)
            mdf_hist.pop(0)

        rms_line.set_data(np.arange(len(rms_hist)), rms_hist)
        mdf_line.set_data(np.arange(len(mdf_hist)), mdf_hist)

        # Autoscale
        for ax in [ax1, ax2, ax3, ax4]:
            ax.relim()
            ax.autoscale_view()

        # ===== STATUS =====
        if frame_count < 60:
            status = "CALIBRATING"
            color = '#ffeaa7'  # soft yellow
        else:
            if fatigue == 1:
                status = "FATIGUED"
                color = '#fab1a0'  # soft red
            else:
                status = "NORMAL"
                color = '#b8e994'  # soft green

        status_box.set_text(
            f"RMS: {rms:.2f}\n"
            f"MDF: {mdf:.2f}\n"
            f"Status: {status}"
        )

        status_box.set_bbox(
            dict(facecolor=color, edgecolor='#333', boxstyle='round,pad=0.5')
        )

        plt.pause(0.01)

    except Exception as e:
        print("Error:", e)
