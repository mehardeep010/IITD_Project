import cv2
import numpy as np

# === Calibration Parameters ===
KNOWN_DIAMETER = 8.0    # cm, real diameter of the pendulum disc
KNOWN_DISTANCE = 25.0   # cm, distance at which calibration image was taken
PIXEL_WIDTH_CALIB = 454 # px, measured diameter of the disc in the calibration image

# Compute focal length (in pixel units)
FOCAL_LENGTH = (PIXEL_WIDTH_CALIB * KNOWN_DISTANCE) / KNOWN_DIAMETER


# === Initialize webcam capture ===
cap = cv2.VideoCapture(0)
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

# === Background subtractor ===
fgbg = cv2.createBackgroundSubtractorMOG2(history=50,
                                          varThreshold=25,
                                          detectShadows=False)

# === State detection parameters ===
min_contour_area = 800      # ignore tiny contours
area_threshold   = 0.20     # 20% area change for state transition

prev_areas    = []          # keep last few contour areas
state         = "Stable"
stable_frames = 0

def get_state_from_areas(areas):
    """Return 'Approaching', 'Receding' or 'Stable' based on area trend."""
    if len(areas) < 4:
        return "Stable"
    recent     = areas[-4:]
    start_avg  = sum(recent[:2]) / 2
    end_avg    = sum(recent[2:]) / 2
    if start_avg == 0:
        return "Stable"
    change_ratio = (end_avg - start_avg) / start_avg
    if change_ratio > area_threshold:
        return "Approaching"
    elif change_ratio < -area_threshold:
        return "Receding"
    else:
        return "Stable"


while True:
    ret, frame = cap.read()
    if not ret:
        break

    # 1) Preprocess
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    blur = cv2.GaussianBlur(gray, (5, 5), 0)

    # 2) Background subtraction + binarize
    fgmask = fgbg.apply(blur)
    _, fgmask = cv2.threshold(fgmask, 200, 255, cv2.THRESH_BINARY)

    # 3) Morphology to clean noise
    kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (5, 5))
    fgmask = cv2.morphologyEx(fgmask, cv2.MORPH_OPEN,  kernel)
    fgmask = cv2.morphologyEx(fgmask, cv2.MORPH_CLOSE, kernel)

    # 4) Find contours
    contours, _ = cv2.findContours(fgmask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

    current_area = 0
    distance_cm  = None

    if contours:
        # pick largest contour by area
        largest = max(contours, key=cv2.contourArea)
        current_area = cv2.contourArea(largest)

        if current_area > min_contour_area:
            # bounding box
            x, y, w, h = cv2.boundingRect(largest)
            cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 3)

            # --- Distance estimation ---
            pixel_width = float(w)
            distance_cm = (KNOWN_DIAMETER * FOCAL_LENGTH) / pixel_width

    # 5) Track area history for state detection
    prev_areas.append(current_area)
    if len(prev_areas) > 6:
        prev_areas.pop(0)

    predicted_state = get_state_from_areas(prev_areas)

    # simple smoothing: require 2 consistent frames to switch away from current state
    if predicted_state == state:
        stable_frames += 1
    else:
        if stable_frames >= 2 or predicted_state == "Stable":
            state = predicted_state
            stable_frames = 0
        else:
            stable_frames = 0

    # 6) Overlay state + distance
    font_scale = 1.0
    thickness  = 2
    # color by state
    if state == "Approaching":
        color = (0, 0, 255)  # red
    elif state == "Receding":
        color = (255, 0, 0)  # blue
    else:
        color = (0, 255, 0)  # green

    # draw state
    cv2.putText(frame, f"Status: {state}", (20, 40),
                cv2.FONT_HERSHEY_SIMPLEX, font_scale, color, thickness)

    # draw distance in black if available
    if distance_cm is not None:
        txt = f"Dist: {distance_cm:.1f} cm"
        cv2.putText(frame, txt, (20, 80),
                    cv2.FONT_HERSHEY_SIMPLEX, font_scale, (0, 0, 0), thickness)

    # 7) Display
    cv2.imshow("Object Detection + Distance", frame)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# cleanup
cap.release()
cv2.destroyAllWindows()
