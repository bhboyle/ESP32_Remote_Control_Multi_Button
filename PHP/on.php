<?php

$output = shell_exec('sudo -u bboyle ssh pi@172.17.17.10 mosquitto_pub -h 172.17.17.10 -u brian -P Hanafranastan1! -t time/sunset -m on');
echo "<pre>$output</pre>";




echo("command sent");

?>