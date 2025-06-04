import cv2
import numpy as np

# --- Initialize webcam capture ---
cap = cv2.VideoCapture(0)
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

# Initialize background subtractor - more aggressive settings
fgbg = cv2.createBackgroundSubtractorMOG2(history=50, 
                                           varThreshold=25, 
                                           detectShadows=False)

# Simple parameters
min_contour_area = 800
area_threshold = 0.20  # 20% area change to trigger state change

# State tracking
prev_areas = []  # Track last few areas
state = "Stable"
stable_frames = 0

def get_state_from_areas(areas):
    """Simple state detection based on area trend"""
    if len(areas) < 4:
        return "Stable"
    
    # Calculate trend over recent frames
    recent = areas[-4:]
    start_avg = sum(recent[:2]) / 2
    end_avg = sum(recent[-2:]) / 2
    
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

    # Simple preprocessing
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    blur = cv2.GaussianBlur(gray, (5, 5), 0)

    # Background subtraction
    fgmask = fgbg.apply(blur)
    _, fgmask = cv2.threshold(fgmask, 200, 255, cv2.THRESH_BINARY)

    # Clean up mask - simple morphology
    kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (5, 5))
    fgmask = cv2.morphologyEx(fgmask, cv2.MORPH_OPEN, kernel)
    fgmask = cv2.morphologyEx(fgmask, cv2.MORPH_CLOSE, kernel)

    # Find contours
    contours, _ = cv2.findContours(fgmask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

    current_area = 0
    
    if contours:
        # Get largest contour
        largest_contour = max(contours, key=cv2.contourArea)
        current_area = cv2.contourArea(largest_contour)
        
        if current_area > min_contour_area:
            # Draw ONLY a simple bounding box - no messy contours
            x, y, w, h = cv2.boundingRect(largest_contour)
            cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 3)

    # Track area history
    prev_areas.append(current_area)
    if len(prev_areas) > 6:  # Keep only recent history
        prev_areas.pop(0)
    
    # Determine state
    predicted_state = get_state_from_areas(prev_areas)
    
    # Simple smoothing - require 2 consistent predictions
    if predicted_state == state:
        stable_frames += 1
    else:
        if stable_frames >= 2 or predicted_state == "Stable":
            state = predicted_state
            stable_frames = 0
        else:
            stable_frames = 0

    # Display state with large, clear text
    font_scale = 1.2
    thickness = 3
    
    # Choose color based on state
    if state == "Approaching":
        color = (0, 0, 255)  # Red
    elif state == "Receding":
        color = (255, 0, 0)  # Blue
    else:
        color = (0, 255, 0)  # Green
    
    cv2.putText(frame, f"Status: {state}", (20, 50), 
                cv2.FONT_HERSHEY_SIMPLEX, font_scale, color, thickness)

    # Show only the main window
    cv2.imshow("Object Detection", frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
