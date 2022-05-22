<?php

$remote = $_GET["remote"];

//$output = shell_exec('sudo -u bboyle ssh pi@172.17.17.10 mosquitto_pub -h 172.17.17.10 -u brian -P Hanafranastan1! -t time/sunset -m on');
//echo "<pre>$output</pre>";


if (isset($_GET['msg'])) {

    $topicTemp = $_GET["topic"];
    $topic = str_replace("$","/",$topicTemp);
    $msg= $_GET["msg"];
    
         
    $output = shell_exec('sudo -u bboyle ssh pi@172.17.17.10 mosquitto_pub -h 172.17.17.10 -u brian -P Hanafranastan1! -t ' .$topic . ' -m ' . $msg);
    
}


if (isset($_GET['wifi'])) {

    $wifi = $_GET["wifi"];     
    $output = shell_exec('sudo -u bboyle ssh pi@172.17.17.10 mosquitto_pub -h 172.17.17.10 -u brian -P Hanafranastan1! -t ' . $remote . '/Wifi/strength -m ' . $wifi);
    
}

if (isset($_GET['battery'])) {

    $battery = $_GET["battery"];     
    
    $output = shell_exec('sudo -u bboyle ssh pi@172.17.17.10 mosquitto_pub -h 172.17.17.10 -u brian -P Hanafranastan1! -t ' . $remote . '/battery/voltage -m ' . $battery);
    
}

echo("battery voltage=".$battery);
echo("Wifi signal strength = ".$wifi);
echo("Commands sent");

?>