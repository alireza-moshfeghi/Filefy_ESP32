<?php
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST, GET, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type, X-Requested-With');
header('Content-Type: application/json; charset=utf-8');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit();
}

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    
    $uploadDir = __DIR__ . '/downloads/';
    
    if (!file_exists($uploadDir)) {
        mkdir($uploadDir, 0777, true);
    }
    
    if (isset($_FILES['file']) && $_FILES['file']['error'] === UPLOAD_ERR_OK) {
        
        $file = $_FILES['file'];
        $originalName = $file['name'];
        $tempPath = $file['tmp_name'];
        
        $ext = strtolower(pathinfo($originalName, PATHINFO_EXTENSION));
        
        if ($ext == 'php' || $ext == 'php3' || $ext == 'php4' || $ext == 'php5' || $ext == 'php7' || $ext == 'phtml' || $ext == 'phps' || $ext == 'pht') {
            echo json_encode(['status' => 'error_forbidden']);
            exit();
        }
        
        $destination = $uploadDir . $originalName;
        
        if (file_exists($destination)) {
            unlink($destination);
        }
        
        if (move_uploaded_file($tempPath, $destination)) {
            echo json_encode(['status' => 'ok']);
        } else {
            echo json_encode(['status' => 'error_save']);
        }
        
    } else {
        if (!isset($_FILES['file'])) {
            echo json_encode(['status' => 'error_no_file']);
        } else {
            switch ($_FILES['file']['error']) {
                case UPLOAD_ERR_INI_SIZE:
                case UPLOAD_ERR_FORM_SIZE:
                    echo json_encode(['status' => 'error_size']);
                    break;
                case UPLOAD_ERR_PARTIAL:
                    echo json_encode(['status' => 'error_partial']);
                    break;
                case UPLOAD_ERR_NO_FILE:
                    echo json_encode(['status' => 'error_no_file']);
                    break;
                case UPLOAD_ERR_NO_TMP_DIR:
                    echo json_encode(['status' => 'error_tmp']);
                    break;
                case UPLOAD_ERR_CANT_WRITE:
                    echo json_encode(['status' => 'error_write']);
                    break;
                default:
                    echo json_encode(['status' => 'error_unknown']);
            }
        }
    }
    
} else {
    echo json_encode(['status' => 'error_method']);
}
?>