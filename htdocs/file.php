<?php
$file_path = '';
if (isset($_GET['name'])) {
    $file_path = 'downloads/' . $_GET['name'];
}

if (file_exists($file_path)) {
    header('Content-Description: File Transfer');
    header('Content-Type: application/octet-stream'); 
    header('Content-Disposition: attachment; filename="' . basename($file_path) . '"');
    header('Expires: 0');
    header('Cache-Control: must-revalidate');
    header('Pragma: public');
    header('Content-Length: ' . filesize($file_path));
    
    ob_clean();
    flush();
    
    readfile($file_path);
    exit;
} else {
    echo "فایل مورد نظر یافت نشد.";
}
?>