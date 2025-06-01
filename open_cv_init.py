import cv2
import numpy as np

# Initialize webcam
cap = cv2.VideoCapture(0)
_, background = cap.read()  # Capture initial background

while True:
    ret, frame = cap.read()
    if not ret:
        break

    # 1. Simple frame differencing (less sensitive than MOG2)
    diff = cv2.absdiff(background, frame)
    gray = cv2.cvtColor(diff, cv2.COLOR_BGR2GRAY)
    _, thresh = cv2.threshold(gray, 25, 255, cv2.THRESH_BINARY)  # Adjust threshold here
    
    # 2. Find only large contours near the camera
    contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    
    for cnt in contours:
        area = cv2.contourArea(cnt)
        if area > 1000:  # Only large objects
            x, y, w, h = cv2.boundingRect(cnt)
            
            # 3. Draw boundary and histogram
            cv2.rectangle(frame, (x, y), (x+w, y+h), (0, 255, 0), 2)
            
            # Create histogram for the detected object
            obj_region = frame[y:y+h, x:x+w]
            if obj_region.size > 0:  # Check if region exists
                color = ('b', 'g', 'r')
                for i, col in enumerate(color):
                    hist = cv2.calcHist([obj_region], [i], None, [256], [0, 256])
                    cv2.normalize(hist, hist, 0, 100, cv2.NORM_MINMAX)
                    for j in range(1, 256):
                        cv2.line(frame, 
                                (x + j, y + h + 30 + int(hist[j])),
                                (x + j-1, y + h + 30 + int(hist[j-1])),
                                (0, 0, 255) if i == 2 else (0, 255, 0) if i == 1 else (255, 0, 0),
                                1)

    # Update background occasionally (every 30 frames)
    if cv2.waitKey(30) == -1:  # Only update when no key pressed
        _, background = cap.read()

    cv2.imshow("Object Detection", frame)
    if cv2.waitKey(1) == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
