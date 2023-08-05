// Copyright Epic Games, Inc. All Rights Reserved.
"use strict"

function getMergeMethodString(mergeMethod) {
	switch(mergeMethod) {
		case "automerge":
			return "Automerge"
		case "initialSubmit":
			return ""
		case "merge_with_conflict":
			return "Merge w/ conflict"
		case "manual_merge":
			return "Manual merge"
		case "populate":
			return "Populate"
	}
	return "UNKNOWN CASE: " + mergeMethod
}

function getMergeMethodColor(mergeMethod) {
	switch(mergeMethod) {
		case "automerge":
			return "black"
		case "initialSubmit":
			return ""
		case "merge_with_conflict":
			return "red"
		case "manual_merge":
			return "darkgray"
		case "populate":
			return "blue"
	}
	return "UNKNOWN CASE: " + mergeMethod	
}

function getStreamGraphName(streamDisplayName) {
	if (streamDisplayName.endsWith("/Main")) {
		return streamDisplayName
	}
	return streamDisplayName.split('/').reverse()[0]
}

function generateChangeList(data) {

	let dataObj = data.data
	let html = '<div style="margin: auto; width: 80%;"><table class="table"><tbody>'
	for (const cl in dataObj.changes) {
		console.log()
		html += '<tr>'
		html += `<td><b>${dataObj.changes[cl].streamDisplayName}</b></td>`
		html += `<td><a href="https://p4-swarm.epicgames.net/changes/${cl}" target="_blank">CL#${cl}</a></td>`
		//html += `<td style="text-align:center;">${getMergeMethodString(dataObj.changes[cl].mergeMethod)}</td>`
		//html += `<td style="text-align:center;">${dataObj.changes[cl].sourceCL}</td>`
		html += '</tr>'
	}
	html += "</tbody></table></div>"

	return html
}

function createGraph(changes) {
	let lines = [
		'digraph robomerge {',
		'fontname="sans-serif"; labelloc=top; fontsize=16;',
		'edge [penwidth=2]; nodesep=.7; ranksep=1.2;',
		'node [shape=box, style=filled, fontname="sans-serif", fillcolor=moccasin];'
	]
	for (let change in changes) {
		const attrs = [
			['label', `<<b>${getStreamGraphName(changes[change].streamDisplayName)}</b><br/>${change}>`],
			['tooltip', `"https://p4-swarm.epicgames.net/changes/${change}"`],
			['URL', `"https://p4-swarm.epicgames.net/changes/${change}"`],
			['target', `"_blank"`],
			['margin', `"0.5,0.1"`],
			['fontsize', 15],
		];
		const attrStrs = attrs.map(([key, value]) => `${key}=${value}`);
		lines.push(`_${change} [${attrStrs.join(', ')}];`);		
	}
	for (let change in changes) {
		if (changes[change].sourceCL in changes) {
			if (changes[change].mergeMethod == "automerge") {
				lines.push(`_${changes[change].sourceCL} -> _${change};`)
			} else {
				lines.push(`_${changes[change].sourceCL} -> _${change} [color=${getMergeMethodColor(changes[change].mergeMethod)}, style=dashed];`)
			}
		}
	}
	lines.push('}')

    const graphContainer = $('<div class="clearfix">');
    const flowGraph = $('<div class="flow-graph" style="display:inline-block;">').appendTo(graphContainer);
    flowGraph.append($('<div>').css('text-align', 'center').text("Building graph..."));
    renderGraph(lines.join('\n'))
        .then(svg => {
        $('#graph-key-template')
            .clone()
            .removeAttr('id')
            .css('display', 'inline-block')
            .appendTo(graphContainer);
        const span = $('<div style="margin: auto; display: flex; justify-content: center;">').html(svg);
        const svgEl = $('svg', span).addClass('branch-graph').removeAttr('width');
        // scale graph to 70% of default size
        const height = Math.round(parseInt(svgEl.attr('height')) * .7);
        svgEl.attr('height', height + 'px').css('vertical-align', 'top');
        flowGraph.empty();
        flowGraph.append(span);
    });
    return graphContainer;	
}

function buildResults(data) {
	if (Object.keys(data.data.changes).length > 0) {
		const $successPanel = $('#success-panel');
		$('#changes', $successPanel).html(generateChangeList(data));
		$successPanel.show();
		$('#graph').append(createGraph(data.data.changes))
	} else {
		const $errorPanel = $('#error-panel');
		$('pre', $errorPanel).html('No results found.')
		$errorPanel.show();
	}
	receivedTrackingResults()
}

function doit(query) {
	$.get(query)
	.then(data => buildResults(data))
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
	$('#loadingDiv').fadeOut("fast", "swing", function() { 
		$('#changes').fadeIn("fast") 
		$('#graph').fadeIn("fast")
	})
}

window.doTrackChange = doit;
