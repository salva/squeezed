/* Functions to control the SqueezeD server
 *
 */


// Low-level writing of functions:
//var dynCmd;
function cmd(command,param)
{
	url = "/dynamic/" + command + "?" + param;
	try {
		dynCmd = window.XMLHttpRequest ? new XMLHttpRequest():new ActiveXObject("Microsoft.XMLHTTP");
		dynCmd.open("GET", url, true);	// a-synchronous call
		//dynCmd.onreadystatechange=dynCmdCloser;
		dynCmd.send(null);
	} catch(ex) {
		alert("squeezed.cmd exception");
	}
}

function wget(url)
{
	dynCmd = window.XMLHttpRequest ? new XMLHttpRequest():new ActiveXObject("Microsoft.XMLHTTP");
	dynCmd.open("GET", url, false);	// synchronous call
	dynCmd.send(null);
	response = dynCmd.responseText;
	alert("wget returns: " + response);
	return dynCmd.responseText;
}


// Player control:
function control(controlCmd, controlParam)
{
	try {
	if (arguments.length < 2)
	{
		cmd("control", "action=" + controlCmd )
	} else {
		cmd("control", "action=" + controlCmd + "?value=" + controlParam)
	}
	} catch( e) { alert("control(): "  + e) }
}



function callback()
{
	var httpRequest;
	this.callback = function(){};
	var that = this;
	

	//Response handler
	this.callbackResponse = function()
	{
		try { 
			if ( httpRequest.readyState == 4 )
			{
				//if  (httpRequest.status == 200) 
				{
					that.callback( httpRequest.responseText, httpRequest.status );
				}
			}
		} catch (failed) {
			alert("callbackResponse() :" + failed);
		}
	}
	

	this.open = function(url, callbackFcn)
	{
		// Code to check for server-side changes
		//Get the url only when the player state is updated
		//alert("setting callback for : " + url);
		try {
			this.callback = callbackFcn;

			httpRequest = new XMLHttpRequest();
			httpRequest.onreadystatechange = this.callbackResponse;
			httpRequest.open("GET", url, true);
			httpRequest.timeout = 3*1000;
			httpRequest.send(null);
		} catch (failed) {
			alert(failed);
		}
	}
} //callback



//---------------------------------------------------------------
// More generic functions:

function setCookie(name, value, seconds) 
{
	if (typeof(seconds) != 'undefined') {
		var date = new Date();
		date.setTime(date.getTime() + (seconds*1000));
		var expires = "; expires=" + date.toGMTString();
	}
	else {
		var expires = "";
	}
	document.cookie = name+"="+value+expires+"; path=/";
}


function getCookie(name)
{
 		name = name + "=";
		var carray = document.cookie.split(';');
 
		for(var i=0;i < carray.length;i++) {
			var c = carray[i];
			while (c.charAt(0)==' ') c = c.substring(1,c.length);
			if (c.indexOf(name) == 0) return c.substring(name.length,c.length);
		}
 		return null;
}



function seconds2str(seconds)
{
	i = parseInt(seconds);
	s = String(i%60); 
	m = Math.round(i/60 - .5);
	if (s.length < 2) 
		s = '0'+s;
	return m+':'+s;
}

//Export functions, to use google-closure on this.
//window['setCookie'] = setCookie;
//window['getCookie'] = getCookie;

//window['control'] = control;
//window['callback'] = callback;
//window['cmd'] = cmd;

