function secsFormat(seconds, format) {

    var parts = [];

    switch (format) {
        case "ms":
            parts = [
                ~~(seconds / 60),             // minutes
                seconds % 60                  // seconds
            ];
            break;
        case "dhms":
            parts = [
                ~~(seconds / 60 / 60 / 24),   // days
                ~~(seconds / 60 / 60) % 24,   // hours
                ~~(seconds / 60) % 60,        // minutes
                seconds % 60                  // seconds
            ];
            break;

    }
    return parts.map(function (v) {
        return (v < 10) ? "0" + v : v;
    }).join(":");
}


$(function () {

    var sprInProgress = false;
    var statusLeft = $("#left");
    var statusRight = $("#right");

    $("button").prop("disabled", true);

    $("#btnStart")
        .on("click", function (e) {
            e.preventDefault();

            var i, spr;
            var pins = "";
            var duration = $("#duration").val() * 60;
            for (i = 1; i <= 4; i++) {
                spr = $("#spr" + i);
                pins += spr.attr("data-sel");
            }

            $.getJSON(
                "/start",
                {pins: pins, duration: duration},
                function (data) {
                    // Nothing to do here as we get feedback as a server event
                }
            ).fail(function (jqXHR) {
                alert("[" + jqXHR.status + "] " + jqXHR.responseText);
            });
        });

    $("#btnStop")
        .on("click", function (e) {
            e.preventDefault();

            $.getJSON(
                "/stop",
                function (data) {
                    // Nothing to do here as we get feedback as a server event
                }
            );
        });

    $(".sprinkler")
        .on("click", function () {
            if (sprInProgress) {
                return;
            }

            // Change clicked sprinkler picto
            var sel = 1 - $(this).attr("data-sel");
            $(this).attr("data-sel", sel);
            $(this).css(
                "background-image",
                sel === 1 ? "url(sprinkler_off.gif)" : "url(sprinkler.gif)"
            );

            // Activate Start button if at least 1 sprinkler is selected
            for (var i = 1; i <= 4; i++) {
                if ($("#spr" + i).attr("data-sel") === "1") {
                    $("#btnStart").prop("disabled", false);
                    break;
                }
            }
        });

    // Populate About dialog
    $.getJSON(
        "/about",
        function (data) {
            $.each(data, function (key, value) {
                if (key === "coreVersion") {
                    value = value.replace(/_/g, ".")
                }
                $("#about_" + key).text(value);
            });
        }
    );


    // Server events handler
    if (!!window.EventSource) {
        var source = new EventSource("/events");

        source.addEventListener("status", function (e) {

            // New ESP status received!

            var i;
            var status = JSON.parse(e.data);

            // Update status bar infos
            statusLeft.html("Uptime: " + secsFormat(status.uptime, "dhms"));
            statusRight.html("Heap: " + status.heap);

            if (status.left) {
                // Currently sprinkling...

                if (!sprInProgress) {
                    // ...but we didn't notice yet

                    // Update flag
                    sprInProgress = true;

                    // Change Start/Stop buttons state
                    $("#btnStart").prop("disabled", true);
                    $("#btnStop").prop("disabled", false);

                    // Update sprinklers pictos
                    for (i = 1; i <= 4; i++) {
                        var spr = $("#spr" + i);
                        if (status.pins[i - 1] === "1") {
                            spr.attr("data-sel", 1);
                            spr.css("background-image", "url(sprinkler_on.gif)");
                        } else {
                            spr.attr("data-sel", 0);
                            spr.css("background-image", "url(sprinkler.gif)");
                        }
                    }

                    // Update drop-down selected value
                    $("#duration").val(status.duration / 60).change();

                }

                // Refresh timer and completion bar
                $("#countdown").html(secsFormat(status.left, "ms"));
                $("#cdInner").width(~~((status.elapsed / status.duration) * 100) + "%");

            } else {
                // Sprinkling has stopped...

                if (sprInProgress) {
                    // ...but we didn't notice yet

                    // Update flag
                    sprInProgress = false;

                    // Set Stop button state to default
                    $("#btnStop").prop("disabled", true);

                    // Set sprinklers pictos to default
                    for (i = 1; i <= 4; i++) {
                        $("#spr" + i)
                            .attr("data-sel", 0)
                            .css("background-image", "url(sprinkler.gif)");
                    }

                    // Set timer and completion bar to default
                    $("#countdown").html("&nbsp;");
                    $("#cdInner").width(0);
                }
            }
        }, false);
    }
});
