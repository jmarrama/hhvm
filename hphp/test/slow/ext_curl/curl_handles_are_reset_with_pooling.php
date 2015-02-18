<?php
ini_set('curl.enableConnectionPooling', true);
$ch = curl_init('foo.bar.com');
curl_close($ch);
$ch = curl_init('therighturl.com');
var_dump(curl_getinfo($ch)['url']);
