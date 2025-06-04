import cv2
import numpy as np
from collections import deque

# Parameters for smoothing and confirmation
WINDOW_SIZE = 10           # Number of frames to average for trend
CONFIRM_FRAMES = 5         # Frames of consistent trend needed to confirm state

# Set up video capture and background subtractor (for motion detection)
cap = cv2.VideoCapture(0)
fgbg = cv2.createBackgroundSubtractorMOG2(history=100, varThreshold=50, detectShadows=False)

# Store recent bounding-box areas
area_history = deque(maxlen=WINDOW_SIZE)
approach_counter = recede_counter = 0
state = None  # Current confirmed state: 'Approaching', 'Receding', or None

while True:
    ret, frame = cap.read()
    if not ret:
        break

    # Detect moving regions using background subtraction
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    fgmask = fgbg.apply(gray)
    _, thresh = cv2.threshold(fgmask, 200, 255, cv2.THRESH_BINARY)
    thresh = cv2.erode(thresh, None, iterations=2)
    thresh = cv2.dilate(thresh, None, iterations=2)

    # Find contours and pick the largest as our obstacle
    contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if contours:
        largest = max(contours, key=cv2.contourArea)
        x, y, w, h = cv2.boundingRect(largest)
        box_area = w * h
        cv2.rectangle(frame, (x, y), (x+w, y+h), (0,255,0), 2)
    else:
        box_area = 0

    # Update sliding window of past areas
    area_history.append(box_area)

    # Check for monotonic trends once we have enough frames
    approaching = receding = False
    if len(area_history) == WINDOW_SIZE:
        # True if each frame’s area >= previous frame’s area
        non_decreasing = all(area_history[i] >= area_history[i-1] for i in range(1, WINDOW_SIZE))
        # True if each frame’s area <= previous frame’s area
        non_increasing = all(area_history[i] <= area_history[i-1] for i in range(1, WINDOW_SIZE))
        if non_decreasing and area_history[-1] > area_history[0]:
            approaching = True
        elif non_increasing and area_history[-1] < area_history[0]:
            receding = True

    # Debounce: require a sequence of frames before confirming
    if approaching:
        recede_counter = 0
        approach_counter += 1
        if approach_counter >= CONFIRM_FRAMES and state != 'Approaching':
            state = 'Approaching'
            print("Approaching confirmed")
            # (Placeholder: trigger a slow-down or avoidance maneuver)
    elif receding:
        approach_counter = 0
        recede_counter += 1
        if recede_counter >= CONFIRM_FRAMES and state != 'Receding':
            state = 'Receding'
            print("Receding confirmed")
            # (Placeholder: resume normal speed or clear avoidance)
    else:
        # No consistent trend detected; reset counters
        approach_counter = recede_counter = 0
        state = None

    # Display state on frame for debugging
    if state:
        cv2.putText(frame, state, (10,30), cv2.FONT_HERSHEY_SIMPLEX, 
                    1, (0,0,255), 2)
    cv2.imshow("Obstacle Detection", frame)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
