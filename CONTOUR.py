import cv2

# --- Initialize webcam capture ---
cap = cv2.VideoCapture(0)
# (Optional) set a fixed frame size for speed
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

# Initialize background subtractor (MOG2)
fgbg = cv2.createBackgroundSubtractorMOG2(history=500, 
                                           varThreshold=50, 
                                           detectShadows=False)

# Parameters for filtering and classification
min_contour_area = 500       # ignore contours smaller than this (noise)
area_change_thresh = 2000    # threshold for area change to consider fast motion
consecutive_frames = 2       # frames required to confirm a state change

# State variables
prev_area = 0
state = "Stable"
approach_count = 0
recede_count = 0
stable_count = 0

while True:
    ret, frame = cap.read()
    if not ret:
        break

    # 1. Preprocessing: grayscale & blur to reduce noise
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    blur = cv2.GaussianBlur(gray, (5, 5), 0)

    # 2. Apply background subtraction to get the moving foreground mask
    fgmask = fgbg.apply(blur)
    # Threshold to clean up residual shadows/dark areas
    _, fgmask = cv2.threshold(fgmask, 200, 255, cv2.THRESH_BINARY)

    # 3. Morphological opening to remove small artifacts:contentReference[oaicite:2]{index=2}
    kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))
    fgmask = cv2.morphologyEx(fgmask, cv2.MORPH_OPEN, kernel, iterations=2)

    # 4. Contour detection on cleaned mask
    contours, _ = cv2.findContours(fgmask, cv2.RETR_EXTERNAL, 
                                   cv2.CHAIN_APPROX_SIMPLE)

    # 5. Find the largest contour by area (if any)
    largest_contour = None
    max_area = 0
    for cnt in contours:
        area = cv2.contourArea(cnt)
        if area > max_area:
            max_area = area
            largest_contour = cnt

    if largest_contour is not None and max_area > min_contour_area:
        # Compute bounding box around the largest moving object
        x, y, w, h = cv2.boundingRect(largest_contour)

        # Enforce a square bounding box by expanding the smaller dimension
        max_side = max(w, h)
        center_x = x + w // 2
        center_y = y + h // 2
        x_new = int(center_x - max_side // 2)
        y_new = int(center_y - max_side // 2)
        # Clamp box within frame boundaries
        x_new = max(0, x_new)
        y_new = max(0, y_new)
        if x_new + max_side > frame.shape[1]:
            x_new = frame.shape[1] - max_side
        if y_new + max_side > frame.shape[0]:
            y_new = frame.shape[0] - max_side

        # Draw the square bounding box on the frame:contentReference[oaicite:3]{index=3}
        cv2.rectangle(frame, (x_new, y_new), 
                             (x_new + max_side, y_new + max_side), 
                             (0, 255, 0), 2)

        # Compute current area of the square box
        current_area = max_side * max_side
        # Compute area change since last frame
        area_diff = current_area - prev_area

        # 6. Preliminary classification based on area change magnitude
        if area_diff > area_change_thresh:
            predicted = "Approaching"
        elif area_diff < -area_change_thresh:
            predicted = "Receding"
        else:
            predicted = "Stable"

        # 7. Smoothing: require consecutive frames to confirm state
        if predicted == "Approaching":
            approach_count += 1
            recede_count = stable_count = 0
        elif predicted == "Receding":
            recede_count += 1
            approach_count = stable_count = 0
        else:
            stable_count += 1
            approach_count = recede_count = 0

        # Update actual state if confirmed
        if approach_count >= consecutive_frames and state != "Approaching":
            state = "Approaching"
        elif recede_count >= consecutive_frames and state != "Receding":
            state = "Receding"
        elif stable_count >= consecutive_frames and state != "Stable":
            state = "Stable"

    else:
        # No significant motion: keep or revert to Stable
        current_area = 0
        stable_count += 1
        approach_count = recede_count = 0
        if stable_count >= consecutive_frames:
            state = "Stable"

    prev_area = current_area

    # 8. Display the current state on the frame
    cv2.putText(frame, state, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 
                1, (0, 255, 255), 2)
    cv2.imshow("Motion Detection", frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
