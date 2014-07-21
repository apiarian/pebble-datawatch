Pebble.addEventListener('ready',
		function(e) {
			console.log('JavasScript app ready and running.');
		}
);

Pebble.addEventListener('appmessage',
		function(e) {
			if(e.payload.gps_request) {
				console.log('Received GPS request: ' + e.payload.gps_request);
				navigator.geolocation.getCurrentPosition(
					function(p) {
						var location_decimals = 1e4;
						Pebble.sendAppMessage(
							{
								'gps_lat_response': ((p.coords.latitude*location_decimals)|0),
								'gps_lon_response': ((p.coords.longitude*location_decimals)|0)
							}, function(e) {
								console.log('Sent GPS');
							}, function(e) {
								console.log('Failed to deliver GPS with error: ' + e.error.message);
							}
						);
					},
					function(error) {
						var lat, lon;
						lon = error.code;
						switch(error.code) {
							case error.TIMEOUT:
								lat = 99;
								break;
							case error.PERMISSION_DENIED:
								lat = 99;
								break;
							case error.POSITION_UNAVAILABLE:
								lat = 99;
								break;
						}
						Pebble.sendAppMessage({'gps_lat_response':lat, 'gps_lon_response':lon},
								function(e) {
									console.log('Sent GPS error code');
								}, function(e) {
									console.log('Failed to deliver GPS error with code: ' + e.error.message);
								}
						);
					},
					{'timeout':1000*5, 'maximumAge':1000*60*10}
				);
			}
		}
);
