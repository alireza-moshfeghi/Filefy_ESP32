<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: POST, GET, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");
header("Content-Type: application/json; charset=utf-8");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit();
}

$uploadDir = __DIR__ . '/uploads/';

if (!is_dir($uploadDir)) {
    mkdir($uploadDir, 0777, true);
}

$allowedExtensions = ['pdf', 'doc', 'docx', 'xls', 'xlsx', 'ppt', 'pptx', 'psd', 'ai', 'dwg', 'dxf', 'jpg', 'jpeg', 'png', 'gif', 'pub'];

$maxFileSize = 100 * 1024 * 1024; 

define('SUCCESS', 200);
define('SUCCESS_PARTIAL', 207);
define('ERROR_NO_FILES', 400);
define('ERROR_FILE_TOO_LARGE', 413);
define('ERROR_INVALID_TYPE', 415);
define('ERROR_UPLOAD_FAILED', 500);
define('ERROR_MOVE_FILE', 501);

if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_FILES['files'])) {
    
    $files = $_FILES['files'];
    $uploadedCount = 0;
    $failedCount = 0;
    $errorCode = SUCCESS;

    for ($i = 0; $i < count($files['name']); $i++) {
        
        $fileName = $files['name'][$i];
        $fileSize = $files['size'][$i];
        $fileError = $files['error'][$i];
        $fileTmpName = $files['tmp_name'][$i];
        $fileExt = strtolower(pathinfo($fileName, PATHINFO_EXTENSION));

        if ($fileError !== UPLOAD_ERR_OK) {
            $failedCount++;
            $errorCode = ERROR_UPLOAD_FAILED;
            continue;
        }

        if ($fileSize > $maxFileSize) {
            $failedCount++;
            $errorCode = ERROR_FILE_TOO_LARGE;
            continue;
        }

        if (!in_array($fileExt, $allowedExtensions)) {
            $failedCount++;
            $errorCode = ERROR_INVALID_TYPE;
            continue;
        }

        $newFileName = basename($fileName);
        $destination = $uploadDir . $newFileName;

        if (move_uploaded_file($fileTmpName, $destination)) {
            $uploadedCount++;
        } else {
            $failedCount++;
            $errorCode = ERROR_MOVE_FILE;
        }
    }

    if ($failedCount > 0 && $uploadedCount > 0) {
        echo json_encode([
            'code' => SUCCESS_PARTIAL,
            'success_count' => $uploadedCount,
            'failed_count' => $failedCount
        ]);
    } 
    elseif ($uploadedCount > 0) {
        echo json_encode([
            'code' => SUCCESS,
            'count' => $uploadedCount
        ]);
    } 
    elseif ($failedCount > 0) {
        echo json_encode([
            'code' => $errorCode
        ]);
    }
    else {
        echo json_encode(['code' => ERROR_NO_FILES]);
    }

} else {
    echo json_encode(['code' => ERROR_NO_FILES]);
}
?>