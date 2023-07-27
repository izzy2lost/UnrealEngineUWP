// Copyright Epic Games, Inc. All Rights Reserved.
"use strict"

function generateChangeList(data) {

	let dataObj = data.data
	console.log(dataObj)
	let html = '<div style="margin: auto; width: 80%;"><table class="table"><tbody>'
	for (const cl in dataObj.changes) {
		console.log()
		html += '<tr>'
		html += `<td><b>${dataObj.changes[cl].streamDisplayName}</b></td>`
		html += `<td><a href="https://p4-swarm.epicgames.net/changes/${cl}" target="_blank">CL#${cl}</a></td>`
		html += '</tr>'
	}
	html += "</tbody></table></div>"

	return html
}

function doit(query) {
	$.get(query)
	.then(data => {
		const $successPanel = $('#success-panel');
		$('#changes', $successPanel).html(generateChangeList(data));
		$('#success-panel').show();
		receivedTrackingResults()
	})
	.catch(error => {
		const $errorPanel = $('#error-panel');
		const errorMsg = error.responseText
			? error.responseText.replace(/\t/g, '    ')
			: 'Internal error: ' + error.message;
		$('pre', $errorPanel).text(errorMsg);
		$errorPanel.show();
		receivedTrackingResults()
	});
}

function receivedTrackingResults(message) {
	$('#loadingDiv').fadeOut("fast", "swing", function() { $('#changes').fadeIn("fast") })
}

window.doTrackChange = doit;
