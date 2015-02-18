<?php
ini_set('curl.enableConnectionPooling', true);
$ch = curl_init('foo.bar.com');
$ch2 = curl_init('www.baz.com');
var_dump(curl_getinfo($ch)['url']);
var_dump(curl_getinfo($ch2)['url']);
curl_close($ch);
curl_close($ch2);
