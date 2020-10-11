#ifndef WEB_THING_HTML_PAGES_H
#define WEB_THING_HTML_PAGES_H

#include "Arduino.h"
#include "WStringStream.h"

const char* HTTP_SELECTED PROGMEM = "selected";
const char* HTTP_CHECKED PROGMEM = "checked";
const char* HTTP_NONE PROGMEM = "none";
const char* HTTP_BLOCK PROGMEM = "block";
const char* HTTP_TRUE PROGMEM = "true";
const char* HTTP_FALSE PROGMEM = "false";

const static char HTTP_HEAD_BEGIN[]         PROGMEM = R"=====(<!DOCTYPE html>
<html lang='en'>
	<head>
		<meta charset="utf-8" />
		<meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'/>
		<title>%s - %s</title>
)=====";

const static char HTTP_HEAD_STYLE[]              PROGMEM = "<link rel=\"stylesheet\" href=\"/css?%s\" />";

const static char HTTP_HEAD_SCRIPT[]             PROGMEM = "<script src=\"/js?%s\"></script>";

const static char HTTP_HEAD_END[]           PROGMEM = R"=====(
	</head>
	<body>
		<div id='bodyDiv'>
)=====";

const static char HTTP_BODY_END[]           PROGMEM = R"=====(
		</div>
	</body>
</html>
)=====";

const static char PAGE_CSS[]           PROGMEM = R"=====(
body{
	text-align: center;
	font-family: arial, sans-serif;
}
#bodyDiv{  
	display:inline-block;
	min-width:350px;
	text-align:left;
}
div{
	background-color:white;
	color:black;
	border:1.0rem;
	border-color:black;
	border-radius:0.3rem;
	background-size: 1em;
	padding:5px;
	text-align:left;
}
input[type='text'],
input[type='password'],
select,
button{
	width: 350px;
}
button{
	border:0;
	border-radius:0.3rem;
	background-color:#1fa3ec;
	color:#fff;
	line-height:2.4rem;
	font-size:1.2rem;
}
.settingstable th{
	font-weight: bold;
}
.settingstable th, .settingstable td{
	border: 1px solid black;
}
.settingstable input[type='text']{
	width: 40px;
}
)=====";

const static char PAGE_JS[]           PROGMEM = R"=====(
function eb(s) {
	return document.getElementById(s);
}
function qs(s) {
	return document.querySelector(s);
}
function sp(i) {
	eb(i).type = (eb(i).type === 'text' ? 'password' : 'text');
}
function c(l){
	eb('s').value=l.innerText||l.textContent;
	eb('p').focus();
}
function hideMqttGroup() {
	var cb = eb('mqttEnabled'); 
	var x = eb('mqttGroup');
	if (cb.checked) {
		x.style.display = 'block';
	} else {
		x.style.display = 'none';
	}
}
)=====";

const static char HTTP_BUTTON[]    PROGMEM = R"=====(
	<div>
		<form action='/%s' method='%s'>
			<button>%s</button>
		</form>
	</div>
)=====";


const static char HTTP_PAGE_CONFIGURATION_STYLE[]    PROGMEM = R"=====(
<style>
#mqttGroup {
	border: 1px solid gray;
	display:%s;
}
</style>
)=====";

const static char HTTP_PAGE_CONFIGURATION_MQTT_BEGIN[]    PROGMEM = R"=====(
	<div id='mqttGroup'>
)=====";

const static char HTTP_PAGE_CONFIGURATION_MQTT_END[]    PROGMEM = R"=====(
	</div>
)=====";

const static char HTTP_PAGE_CONFIGURATION_OPTION[]    PROGMEM = R"=====(
	<div>
		<label>
			<input type='checkbox' name='%s' value='true' %s %s>
			%s
		</label>
	</div>
)=====";

const static char HTTP_PAGE_CONFIIGURATION_OPTION_MQTTHASS[] PROGMEM = "Support Autodiscovery for Home Assistant using MQTT<br>";
const static char HTTP_PAGE_CONFIIGURATION_OPTION_MQTT[] PROGMEM = "Support MQTT";
const static char HTTP_PAGE_CONFIIGURATION_OPTION_APFALLBACK[] PROGMEM = "Enable Fallback to AP-Mode if WiFi Connection gets lost";
const static char HTTP_PAGE_CONFIIGURATION_OPTION_MQTTSINGLEVALUES[] PROGMEM = "Send all values also as single values via MQTT";

const static char HTTP_SAVED[]              PROGMEM = R"=====(
<div>
	%s
</div>
<div>
	%s
</div>
)=====";

const static char HTTP_HOME_BUTTON[]              PROGMEM = R"=====(
<div>
	<form action='/config' method='get'>
		<button>Back to Home</button>
	</form>
</div>
)=====";

const static char HTTP_FORM_FIRMWARE[] PROGMEM = R"=====(
<form method='POST' action='' enctype='multipart/form-data'>
	<div>
		<input type='file' accept='.bin, .bin.gz' name='update'>
	</div>
	<div>
		<button type='submit'>Update firmware</button>
	</div>
</form>
)=====";

const static char HTTP_CONFIG_PAGE_BEGIN[]         PROGMEM = R"=====(
<form method='get' action='save_%s'>
)=====";

const static char HTTP_TEXT_FIELD[]    PROGMEM = R"=====(
	<div>
		%s<br>
		<input type='text' name='%s' maxlength=%d value='%s'>
	</div>
)=====";

const static char HTTP_TEXT_FIELD_INTEGER[]    PROGMEM = R"=====(
	<div>
		%s<br>
		<input type='text' name='%s' maxlength=%d value='%d'>
	</div>
)=====";

const static char HTTP_PASSWORD_FIELD[]    PROGMEM = R"=====(
	<div>
		<label>%s <small><input type="checkbox" onclick="sp('%s')"> (show password)</small></label><br>
		<input type='password' name='%s' id='%s' maxlength=%d value='%s'>
	</div>
)====="; 

const static char HTTP_CHECKBOX[]         PROGMEM = R"=====(					
		<div>
			<label>
				<input type='checkbox' name='%s' value='true' %s>%s
			</label>
		</div>
)=====";

const static char HTTP_CHECKBOX_OPTION[]    PROGMEM = R"=====(
	<div>
		<label>
			%s<br>
			<input type='checkbox' id='%s' name='%s' value='true' %s onclick='%s'>%s
		</label>
	</div>
)=====";

const static char HTTP_COMBOBOX_BEGIN[]         PROGMEM = R"=====(
		<div>
			%s<br>
			<select name='%s'>
)=====";
const static char HTTP_COMBOBOX_ITEM[]         PROGMEM = R"=====(        		
				<option value='%s' %s>%s</option>                  
)=====";
const static char HTTP_COMBOBOX_END[]         PROGMEM = R"=====(					
			</select>
		</div>
)=====";

const static char HTTP_CONFIG_SAVE_BUTTON[]         PROGMEM = R"=====(	
		<div>
			<button type='submit'>Save configuration</button>
		</div>
</form>
)=====";
const static char HTTP_CONFIG_SAVEANDREBOOT_BUTTON[]         PROGMEM = R"=====(	
		<div>
			<button type='submit'>Save config and reboot</button>
		</div>
</form>
)=====";
const static char HTTP_THERM_INFO[]         PROGMEM = R"=====(	
		<br>
		If you have any questions see documentation at<br>
		<a href="https://github.com/fashberg/WThermostatBeca/blob/master/Configuration.md" target="_blank">https://github.com/fashberg/WThermostatBeca/blob/master/Configuration.md</a>
		<br>
		<br>
)=====";
const static char HTTP_THERM_DEADZONE[]         PROGMEM = R"=====(	
		<small>Hint: Set this paramter to the same value which is confired at thermostat settings. Value between 1 and 5 degree Celsius, default is 1. If you do not have the Relay Hardware hack we need to know this 
		setting to calculate the state of the Heating Relay</small><br>
</form>
)=====";


#ifndef MINIMAL
#define favicon_ico_gz_len 1057
const uint8_t favicon_ico_gz[] PROGMEM = {
0x1f, 0x8b, 0x08, 0x00, 0xb3, 0x28, 0x6a, 0x5f, 0x02, 0x03, 0xed, 0x94,
0xd7, 0x53, 0x24, 0x45, 0x1c, 0xc7, 0xbf, 0x9c, 0xa2, 0xa8, 0xb8, 0x4b,
0x92, 0xf3, 0x80, 0x93, 0x25, 0x73, 0x18, 0x38, 0xc0, 0xdb, 0x1d, 0x75,
0x74, 0x4e, 0x14, 0x15, 0x8e, 0x45, 0x3d, 0xef, 0xd8, 0x25, 0xb9, 0x06,
0x0e, 0x4f, 0x04, 0x01, 0x71, 0x41, 0xe7, 0x3f, 0x20, 0x53, 0x45, 0x15,
0x2f, 0xbc, 0x50, 0x45, 0x2e, 0x1e, 0xc8, 0x14, 0x45, 0xce, 0xb1, 0x24,
0x48, 0x91, 0x97, 0x9c, 0x73, 0xce, 0xc9, 0x9e, 0x5d, 0x10, 0x30, 0x3c,
0xeb, 0xc3, 0x7d, 0xa6, 0xbe, 0x3d, 0x3d, 0xfd, 0xa9, 0x5f, 0xcf, 0x74,
0x75, 0xd7, 0x00, 0x6a, 0xe4, 0x12, 0x08, 0x40, 0x5a, 0x0d, 0x64, 0x6a,
0x00, 0xfa, 0x00, 0xac, 0x49, 0xc8, 0x10, 0xee, 0x42, 0x35, 0xfe, 0x6f,
0x44, 0x44, 0x44, 0x20, 0x32, 0x32, 0x12, 0x51, 0x51, 0x51, 0x88, 0x8e,
0x8e, 0x46, 0x4c, 0x4c, 0x0c, 0x62, 0x63, 0x63, 0x11, 0x17, 0x17, 0x87,
0xf8, 0xf8, 0x78, 0x24, 0x24, 0x24, 0x20, 0x31, 0x31, 0x11, 0x49, 0x49,
0x49, 0x48, 0x4e, 0x4e, 0x46, 0x4a, 0x4a, 0x0a, 0x52, 0x52, 0x53, 0x91,
0x4a, 0x92, 0x96, 0x96, 0x86, 0xf4, 0xf4, 0x74, 0x64, 0x64, 0x64, 0x20,
0x33, 0x33, 0x13, 0x59, 0x59, 0x59, 0xc8, 0xce, 0xce, 0x46, 0x4e, 0x4e,
0x0e, 0x72, 0x73, 0x73, 0x91, 0x97, 0x97, 0x87, 0xfc, 0xfc, 0x7c, 0x14,
0x14, 0x14, 0xa0, 0xb0, 0xb0, 0x10, 0x45, 0x45, 0x45, 0x28, 0x2e, 0x2e,
0x46, 0x49, 0x49, 0x09, 0x4a, 0x4b, 0x4b, 0x51, 0x56, 0x56, 0x86, 0xf2,
0xf2, 0x72, 0x54, 0x54, 0x54, 0xa0, 0xb2, 0xb2, 0x12, 0x55, 0x55, 0x55,
0xa8, 0xae, 0xae, 0x46, 0x4d, 0x4d, 0x0d, 0x6a, 0x6b, 0x6b, 0x51, 0x57,
0x57, 0x87, 0xfa, 0xfa, 0x7a, 0x34, 0x34, 0x34, 0xa0, 0xb1, 0xb1, 0x11,
0x4d, 0x4d, 0x4d, 0x68, 0x6e, 0x6e, 0x46, 0x4b, 0x4b, 0x0b, 0x5a, 0x5b,
0x5b, 0xd1, 0xd6, 0xd6, 0x86, 0xf6, 0xf6, 0x76, 0x74, 0x74, 0x74, 0xa0,
0xb3, 0xb3, 0x13, 0x5d, 0x5d, 0x5d, 0xe8, 0xee, 0xee, 0x46, 0x4f, 0x4f,
0x0f, 0x7a, 0x7b, 0x7b, 0xd1, 0xd7, 0xd7, 0x87, 0xfe, 0xfe, 0x7e, 0x0c,
0xf4, 0x0f, 0xa2, 0x6f, 0x60, 0x00, 0xfd, 0x24, 0x03, 0x24, 0x7d, 0x83,
0x83, 0x18, 0x20, 0x19, 0x24, 0xf9, 0x7d, 0x68, 0x00, 0x0d, 0x8a, 0x21,
0x0c, 0x0d, 0x0d, 0xa1, 0x66, 0x64, 0x08, 0xbf, 0x0d, 0x2b, 0xa0, 0x50,
0x28, 0xd0, 0x38, 0xa2, 0x40, 0xfd, 0xc8, 0x30, 0x1a, 0x46, 0x15, 0x18,
0x1e, 0x1e, 0xc6, 0xc8, 0xc8, 0x08, 0x46, 0x47, 0x47, 0x31, 0x36, 0x36,
0x86, 0xf1, 0xf1, 0x71, 0x4c, 0x4c, 0x4c, 0x60, 0x92, 0x64, 0x6a, 0x72,
0x02, 0x13, 0x93, 0x93, 0x98, 0x24, 0x99, 0x9a, 0x9a, 0xc2, 0xf4, 0xf4,
0x34, 0x66, 0x66, 0x66, 0x30, 0x3b, 0x3b, 0x8b, 0xb9, 0xb9, 0x39, 0xcc,
0xcf, 0xcf, 0x63, 0x61, 0x61, 0x01, 0x8b, 0x8b, 0x8b, 0x58, 0x5a, 0x5a,
0xc2, 0xf2, 0xf2, 0x32, 0x56, 0x56, 0x56, 0xb0, 0xb2, 0xba, 0x8a, 0x55,
0x92, 0xb5, 0xd5, 0x35, 0xac, 0x93, 0xac, 0xae, 0x91, 0xbe, 0x32, 0xe4,
0x79, 0x7d, 0x1d, 0x1b, 0x1b, 0x1b, 0xd8, 0xd8, 0xdc, 0xc4, 0x26, 0xc9,
0xc6, 0xd6, 0x26, 0xb6, 0xb6, 0xb6, 0xb0, 0xbd, 0xbd, 0x8d, 0x9d, 0x9d,
0x1d, 0xec, 0xee, 0xee, 0x92, 0xec, 0x61, 0x6f, 0x6f, 0x0f, 0xfb, 0xfb,
0xfb, 0x38, 0x38, 0x38, 0xc0, 0xc1, 0xe1, 0x21, 0x0e, 0xb9, 0x1c, 0x1d,
0xe1, 0x88, 0xe4, 0xf8, 0xf8, 0x18, 0x27, 0x27, 0x27, 0x38, 0x3e, 0x3d,
0xc1, 0xc9, 0xe9, 0x29, 0x4e, 0x49, 0x9e, 0xf0, 0x84, 0xbf, 0xc2, 0x72,
0x04, 0xf8, 0x7a, 0x72, 0x78, 0x11, 0xbc, 0xbd, 0x7d, 0x38, 0x64, 0x32,
0x9f, 0x60, 0xf6, 0x8c, 0xd0, 0xc7, 0x6e, 0x77, 0x6c, 0x2c, 0xcc, 0xcd,
0x2d, 0xb8, 0x58, 0x5a, 0x59, 0x59, 0x59, 0x5b, 0xdf, 0xb2, 0x79, 0xcd,
0x5e, 0x12, 0x1c, 0xae, 0xd4, 0x61, 0x8f, 0x5c, 0xed, 0x6e, 0x59, 0x98,
0x72, 0x98, 0x98, 0x98, 0x98, 0xaa, 0x30, 0x33, 0xb7, 0x11, 0x7a, 0x04,
0xa9, 0xca, 0xdd, 0x6d, 0x0d, 0xf5, 0x74, 0x75, 0xce, 0x20, 0x1d, 0x5d,
0x82, 0x8e, 0xae, 0xbe, 0x19, 0xed, 0xa7, 0xf4, 0xc1, 0xae, 0x66, 0xd7,
0xb5, 0xb5, 0xf8, 0x3c, 0x1e, 0x9f, 0xcf, 0xd3, 0xe4, 0xf1, 0x34, 0x35,
0xb9, 0x86, 0xaf, 0xf3, 0xd2, 0x4d, 0x3b, 0x1f, 0xe5, 0x0b, 0x82, 0x5d,
0x8c, 0xaf, 0x3f, 0xf7, 0xac, 0xfa, 0xd3, 0x84, 0xa7, 0xae, 0xa9, 0xa9,
0xe1, 0x9a, 0xba, 0xba, 0xfa, 0x33, 0x1a, 0x7a, 0x66, 0x66, 0xb6, 0x5e,
0x67, 0x5e, 0xc0, 0x7f, 0x41, 0x47, 0x5b, 0x8b, 0xc7, 0xd3, 0xd2, 0xe6,
0xbf, 0x48, 0x8a, 0xb9, 0x39, 0x78, 0x7a, 0xc6, 0xa6, 0xe7, 0xfe, 0x9e,
0x40, 0x9f, 0xa2, 0x55, 0xbc, 0xfb, 0xc9, 0x87, 0x0c, 0x43, 0x33, 0x0c,
0xe3, 0xe8, 0x28, 0xbc, 0xe4, 0x0d, 0x68, 0x46, 0x4c, 0xd3, 0x0c, 0xfd,
0xf6, 0x47, 0xef, 0xbd, 0xff, 0x0e, 0xfd, 0x26, 0x4d, 0x8b, 0x84, 0x34,
0x7d, 0xc5, 0x53, 0x0c, 0x45, 0xe6, 0xa0, 0xee, 0x7e, 0xfc, 0xc1, 0x5b,
0xf4, 0xd9, 0x64, 0x17, 0xde, 0xd8, 0x90, 0x66, 0x1e, 0xb8, 0x89, 0xc5,
0x6e, 0x9f, 0x7e, 0xfe, 0x19, 0xb9, 0xb9, 0x8a, 0x5d, 0x5d, 0x9d, 0x89,
0x7f, 0xc3, 0xf3, 0xdc, 0x1b, 0xd1, 0xce, 0xf2, 0x1b, 0x96, 0x46, 0x76,
0xca, 0xf5, 0x86, 0x18, 0x05, 0xb0, 0xac, 0xdf, 0x65, 0x2f, 0x30, 0xa2,
0x9c, 0xc2, 0x9d, 0x24, 0x76, 0x21, 0x2c, 0xeb, 0xe1, 0xe1, 0xf9, 0x90,
0xf1, 0xf4, 0xe6, 0xbc, 0xc9, 0x15, 0xaf, 0xda, 0x8a, 0x47, 0x81, 0x2c,
0x6b, 0x28, 0x67, 0x55, 0xf5, 0xaf, 0xff, 0xdd, 0x7b, 0xc9, 0xad, 0xa5,
0x76, 0x2e, 0xf7, 0x24, 0x9c, 0xb7, 0xbe, 0xa8, 0x37, 0x14, 0x39, 0x9f,
0xf9, 0x40, 0x93, 0xf0, 0xd0, 0x57, 0xc4, 0x4e, 0x9c, 0x37, 0xff, 0xd3,
0x9b, 0x18, 0x51, 0xc4, 0xdb, 0x7b, 0xb0, 0xec, 0x0f, 0x61, 0x0e, 0x56,
0xba, 0x9e, 0xf2, 0x60, 0xce, 0x9b, 0x5d, 0x78, 0x03, 0xce, 0x07, 0xca,
0xd9, 0x10, 0x77, 0xd6, 0xe1, 0xe5, 0xe7, 0xa5, 0x9e, 0xa1, 0x92, 0x2b,
0x5e, 0x60, 0x40, 0x39, 0x31, 0x0e, 0x5c, 0x3f, 0x48, 0x2c, 0x0f, 0x93,
0x07, 0x09, 0xef, 0x93, 0x7a, 0xea, 0x92, 0x37, 0xa4, 0x18, 0x7f, 0xd5,
0x71, 0x72, 0x71, 0x0a, 0x37, 0x0c, 0x67, 0xbc, 0xc2, 0xae, 0xd6, 0x1b,
0x51, 0x8c, 0xea, 0xfb, 0xfc, 0xbd, 0x64, 0x1e, 0xce, 0x5f, 0x04, 0x48,
0x03, 0x1f, 0x93, 0xef, 0xbb, 0xbc, 0x3e, 0x5a, 0x75, 0x54, 0x6e, 0xf8,
0xb1, 0x01, 0x6e, 0xdc, 0x28, 0xf1, 0x16, 0xe7, 0xde, 0x55, 0x70, 0x93,
0xa2, 0xa4, 0x52, 0xa9, 0x44, 0x22, 0x71, 0x72, 0x57, 0xf1, 0xd0, 0x8d,
0xac, 0xff, 0xcc, 0x87, 0x88, 0x4d, 0x2d, 0x45, 0x64, 0xf7, 0xa8, 0x73,
0x54, 0xdb, 0xe7, 0x68, 0x75, 0xee, 0xef, 0x5b, 0xda, 0x3a, 0x70, 0x83,
0xdc, 0xc9, 0xe0, 0x5a, 0x0e, 0x72, 0x04, 0x5e, 0xbd, 0xed, 0xad, 0xf4,
0xa1, 0x52, 0x7b, 0x3b, 0xfb, 0x7f, 0xe0, 0x36, 0xed, 0xa7, 0xf4, 0xe1,
0xfe, 0x0f, 0x18, 0x91, 0x48, 0x28, 0x14, 0xde, 0x11, 0x2a, 0x6f, 0x4a,
0x44, 0x22, 0x91, 0xb3, 0x4f, 0x28, 0xa7, 0x7f, 0xfd, 0xe5, 0xe7, 0x1f,
0xbf, 0xfb, 0xea, 0x4b, 0xd9, 0x37, 0xbe, 0xbe, 0xbe, 0xdf, 0x92, 0x7c,
0x4d, 0xe2, 0x2b, 0x93, 0xc9, 0x7c, 0xbf, 0xff, 0x89, 0xfd, 0x1f, 0xf0,
0x5f, 0xff, 0x01, 0xff, 0x00, 0xc7, 0x38, 0x6b, 0x38, 0xbe, 0x08, 0x00,
0x00 };
#endif
const char* STR_BYTE PROGMEM = " Byte";

void htmlTableRowTitle(AsyncResponseStream* page,  char const* title){
	page->print(F("<tr><th>"));
	page->print(title);
	page->print(F("</th><td>"));
}
void htmlTableRowTitle(AsyncResponseStream* page,  const __FlashStringHelper * title){
	page->print(F("<tr><th>"));
	page->print(title);
	page->print(F("</th><td>"));
}

void htmlTableRowEnd(AsyncResponseStream* page){
	page->print(F("</td></tr>"));
}

#endif
