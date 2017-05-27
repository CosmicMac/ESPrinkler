var terminal, ota, otaInProgress = false;

function putLine(line) {
    $("<pre>" + line + "</pre>")
        .appendTo(terminal)[0]
        .scrollIntoView();
}

$(function () {
    terminal = $("#terminal");

    $("#btnDelete")
        .on("click", function () {
            terminal.empty();
        });

    if (!!window.EventSource) {
        var source = new EventSource("/events");

        source.addEventListener("open", function (e) {
            putLine("Events connected");
        }, false);

        source.addEventListener("error", function (e) {
            if (e.target.readyState !== EventSource.OPEN) {
                putLine("Events disconnected");
            }
        }, false);

        source.addEventListener("message", function (e) {
            putLine(e.data);
        }, false);
        
        source.addEventListener("ota", function (e) {
            if (e.data === "start") {
                putLine("OTA update in progress");
                ota = $("<pre class='ota'></pre>").appendTo(terminal);
                ota[0].scrollIntoView();
                otaInProgress = true;
            } else if (e.data === "end") {
                putLine("Done! Restarting...");
                setTimeout(function () {
                    location.reload();
                }, 5000);
            }
            else {
                var nb = Math.floor(e.data / 4);
                ota.text("█".repeat(nb) + "░".repeat(25 - nb) + " " + e.data + "%");
            }
        }, false);
    }
});
