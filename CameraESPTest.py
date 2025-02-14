import cv2
import numpy as np
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from email.mime.base import MIMEBase
from email import encoders
import requests
import time
from ultralytics import YOLO
import os

# Cấu hình tài khoản Gmail của bạn
sender_email = "xxx@gmail.com"  # Email gửi từ
receiver_email = "xxx@gmail.com"  # Email nhận
password = "MKungdung"  # Mật khẩu ứng dụng (không phải mật khẩu Gmail thông thường)

# Cấu hình server SMTP của Gmail
smtp_server = "smtp.gmail.com"
smtp_port = 587

# Địa chỉ IP của ESP32-CAM
ESP32_CAM_IP = "http://192.168.45.10"  # Địa chỉ IP ESP32-CAM

# Địa chỉ URL cho stream video
stream_url = f"{ESP32_CAM_IP}/stream"

# Tải mô hình YOLOv8 (pre-trained)
model = YOLO('yolov8n.pt')  # Bạn có thể thay thế 'yolov8n.pt' bằng mô hình khác nếu cần

# Tạo thư mục Data để lưu trữ ảnh
output_directory = "Data"
os.makedirs(output_directory, exist_ok=True)

# Biến để đánh số thứ tự ảnh
image_counter = 0

# Hàm gửi email thông báo kèm ảnh
def send_email_with_image(image_path):
    subject = "Thông báo: Có người lạ"
    body = "Cảnh báo! Có người lạ xuất hiện. Xem hình ảnh đính kèm."

    # Tạo email
    msg = MIMEMultipart()
    msg['From'] = sender_email
    msg['To'] = receiver_email
    msg['Subject'] = subject
    msg.attach(MIMEText(body, 'plain'))

    # Đính kèm ảnh
    with open(image_path, "rb") as attachment:
        part = MIMEBase('application', 'octet-stream')
        part.set_payload(attachment.read())
    encoders.encode_base64(part)
    part.add_header('Content-Disposition', f'attachment; filename={os.path.basename(image_path)}')
    msg.attach(part)

    try:
        # Kết nối tới server SMTP của Gmail
        server = smtplib.SMTP(smtp_server, smtp_port)
        server.starttls()  # Bật mã hóa TLS
        server.login(sender_email, password)  # Đăng nhập vào Gmail
        server.sendmail(sender_email, receiver_email, msg.as_string())  # Gửi email
        print("Thông báo đã được gửi qua email!")
    except Exception as e:
        print(f"Đã xảy ra lỗi khi gửi email: {e}")
    finally:
        server.quit()

# Khởi tạo OpenCV để hiển thị video
try:
    response = requests.get(stream_url, stream=True, timeout=10)
    if response.status_code == 200:
        print("Đang nhận dữ liệu từ ESP32-CAM...")

        bytes_data = b""
        for chunk in response.iter_content(chunk_size=1024):
            bytes_data += chunk
            a = bytes_data.find(b"\xff\xd8")  # Tìm vị trí bắt đầu JPEG
            b = bytes_data.find(b"\xff\xd9")  # Tìm vị trí kết thúc JPEG

            if a != -1 and b != -1:
                jpg_data = bytes_data[a:b + 2]  # Cắt JPEG frame
                bytes_data = bytes_data[b + 2:]  # Xóa frame đã xử lý

                # Chuyển đổi dữ liệu JPEG thành hình ảnh
                img = cv2.imdecode(np.frombuffer(jpg_data, dtype=np.uint8), cv2.IMREAD_COLOR)
                if img is not None:
                    # Nhận diện đối tượng với YOLOv8
                    results = model(img)

                    # Duyệt qua các đối tượng phát hiện
                    person_detected = False
                    for result in results[0].boxes.data:  # Lấy tất cả các đối tượng phát hiện
                        if result[5] == 0:  # class 0 là con người trong COCO dataset
                            person_detected = True
                            x1, y1, x2, y2 = map(int, result[:4])
                            cv2.rectangle(img, (x1, y1), (x2, y2), (0, 255, 0), 2)
                            cv2.putText(img, 'Person', (x1, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 255, 0), 2)

                    # Nếu phát hiện con người, lưu ảnh và gửi email
                    if person_detected:
                        print("Có người lạ!")
                        image_path = os.path.join(output_directory, f"detected_person_{image_counter}.jpg")
                        cv2.imwrite(image_path, img)  # Lưu ảnh
                        send_email_with_image(image_path)  # Gửi email kèm ảnh
                        image_counter += 1

                    # Hiển thị ảnh từ video stream
                    cv2.imshow("ESP32-CAM Live Stream with YOLOv8", img)
                    if cv2.waitKey(1) & 0xFF == ord('q'):  # Nhấn 'q' để thoát
                        break

                    # Thêm delay để giảm FPS
                    time.sleep(0.1)  # 100ms = 10 FPS

    else:
        print(f"Lỗi kết nối với ESP32-CAM. Mã lỗi: {response.status_code}")
except requests.exceptions.RequestException as e:
    print(f"Đã xảy ra lỗi kết nối: {e}")

# Đóng tất cả cửa sổ khi thoát
cv2.destroyAllWindows()
