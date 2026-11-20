#!/usr/bin/perl
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 deuter0n
#
# Splits the approved single-file mockup into the app's UI files:
#   ui/index.html  markup
#   ui/app.css     styles, with the bundled fonts instead of Google Fonts
#   ui/app.js      the UI; talks to the host through window.chrome.webview
#   ui/mock.js     browser-only: simulated host, "Simulate" buttons, tray demo
#                  (left out of the Release build)
#
# Usage: perl tools/split_mockup.pl <mockup/index.html> <ui dir>
use strict;
use warnings;

my ($src, $out) = @ARGV;
die "usage: split_mockup.pl <mockup.html> <ui dir>\n" unless $src && $out;
open my $fh, '<:raw', $src or die "$src: $!";
my @lines = <$fh>;
close $fh;
my $all = join '', @lines;

sub between {
    my ($text, $start, $end) = @_;
    my $i = index($text, $start);
    die "marker not found: $start\n" if $i < 0;
    my $j = index($text, $end, $i + length $start);
    die "marker not found: $end\n" if $j < 0;
    return substr($text, $i + length $start, $j - $i - length $start);
}
sub cut {  # removes $piece from $$text (must exist exactly once)
    my ($text, $piece) = @_;
    my $i = index($$text, $piece);
    die "piece not found:\n$piece\n" if $i < 0;
    substr($$text, $i, length $piece) = '';
}
sub replace {
    my ($text, $from, $to) = @_;
    my $i = index($$text, $from);
    die "text not found:\n$from\n" if $i < 0;
    substr($$text, $i, length $from) = $to;
}
sub write_file {
    my ($name, $text) = @_;
    open my $o, '>:raw', "$out/$name" or die "$out/$name: $!";
    print $o $text;
    close $o;
}

my $header = "SPDX-License-Identifier: GPL-3.0-or-later\n   Copyright (C) 2026 deuter0n\n   Generated from the approved mockup by tools/split_mockup.pl.";

# ---------------------------------------------------------------- CSS
my $css = between($all, "<style>\n", "</style>");
my $fonts = <<'CSS';
/* Bundled fonts (SIL Open Font License 1.1, see fonts/OFL-*.txt). */
@font-face { font-family: 'Chakra Petch'; font-weight: 500; font-style: normal; font-display: swap; src: url(fonts/ChakraPetch-Medium.woff2) format('woff2'); }
@font-face { font-family: 'Chakra Petch'; font-weight: 600; font-style: normal; font-display: swap; src: url(fonts/ChakraPetch-SemiBold.woff2) format('woff2'); }
@font-face { font-family: 'Chakra Petch'; font-weight: 700; font-style: normal; font-display: swap; src: url(fonts/ChakraPetch-Bold.woff2) format('woff2'); }
@font-face { font-family: 'Inter'; font-weight: 100 900; font-style: normal; font-display: swap; src: url(fonts/Inter.woff2) format('woff2'); }
@font-face { font-family: 'JetBrains Mono'; font-weight: 100 800; font-style: normal; font-display: swap; src: url(fonts/JetBrainsMono.woff2) format('woff2'); }

/* The UI shows and hides elements with the hidden attribute; make it win
   over display rules, and drop the browser.s default page margin (the mockup.s
   artifact host added both rules itself). */
[hidden] { display: none !important; }
body { margin: 0; }

CSS
my $inApp = <<'CSS';

/* ---------- inside the app ---------- */
/* The page is the window there: no page margin or drawn window frame, and
   the header is the title bar (drag to move, double-click to maximize). */
html.in-app .stage { max-width: none; padding: 0; gap: 0; }
html.in-app body { overflow: hidden; }  /* the window never scrolls; views slide in without a scrollbar flash */
html.in-app .app { border: 0; border-radius: 0; box-shadow: none; height: 100vh; }
html.in-app .titlebar { border-radius: 0; app-region: drag; }
html.in-app .titlebar button, html.in-app .titlebar select, html.in-app .titlebar input,
html.in-app .titlebar a, html.in-app .titlebar [role="menu"] { app-region: no-drag; }
CSS
write_file('app.css', "/* $header */\n\n$fonts$css$inApp");

# ---------------------------------------------------------------- markup
my $body = between($all, "</style>\n", "<script>\n");

# Mock-only markup moves to mock.js.
my $note = between($body, "\n  <p class=\"note\">", "</p>\n");
$note = "<p class=\"note\">$note</p>";
my $demo = between($body, "\n  <div class=\"demo\"", "</div>\n");
$demo = "<div class=\"demo\"$demo</div>";
my $tray = "<div class=\"tray\"" . between($body, "\n<div class=\"tray\"", "</div>\n") . "</div>";
cut(\$body, "\n  $note\n");
cut(\$body, "  $demo\n");
cut(\$body, "\n$tray\n");
replace(\$body, '<span>Interactive mockup · simulated audio</span>',
        '<span id="aboutNote">Settings are saved automatically.</span>');

my $html = <<"HTML";
<!doctype html>
<!-- $header -->
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Graviton FX</title>
<link rel="stylesheet" href="app.css">
</head>
<body>
$body
<!-- mock.js exists only in the browser preview; the app serves app.js alone. -->
<script src="mock.js"></script>
<script src="app.js"></script>
</body>
</html>
HTML
write_file('index.html', $html);

# ---------------------------------------------------------------- scripts
my $js = between($all, "<script>\n", "</script>");

# The mock host function moves out whole.
my $mockStart = index($js, "function createMockHost() {");
my $mockEndMarker = "  return { send, on: (fn) => listeners.push(fn) };\n}\n";
my $mockEnd = index($js, $mockEndMarker, $mockStart);
die "mock host not found\n" if $mockStart < 0 || $mockEnd < 0;
my $mockHost = substr($js, $mockStart, $mockEnd + length($mockEndMarker) - $mockStart);
substr($js, $mockStart, length $mockHost) = '';

replace(\$js, "  return createMockHost();\n", "  return window.createMockHost();  // mock.js, browser preview only\n");
replace(\$js, "const bridge = (() => {\n",
        "if (window.chrome && window.chrome.webview) document.documentElement.classList.add(\"in-app\");\n\nconst bridge = (() => {\n");

# Simulate buttons: wired inside the mock host instead.
my $demoWiring = '';
for my $line (split /\n/, $js) {
    $demoWiring .= "$line\n" if $line =~ /^\$\("demo\w+"\)\.addEventListener/;
}
cut(\$js, $demoWiring);

# Simulate button labels: only when the (mock) host sends them.
my $demoRender = between($js, "  // mockup-only simulate buttons\n", "\n\n");
cut(\$js, "  // mockup-only simulate buttons\n$demoRender\n");
replace(\$js, "  // updates: dot on the Settings button",
        "  // mock host only: simulate button labels\n  if (s.demo && window.mockRender) window.mockRender(s);\n\n  // updates: dot on the Settings button");

# Window buttons: the host minimizes or closes the real window; the browser
# preview shows the tray demo.
my $trayDemo = between($js, "/* ---------- tray demo ---------- */\n", "\n\n");
cut(\$js, "/* ---------- tray demo ---------- */\n$trayDemo\n");
my $windowButtons = <<'JS';
/* ---------- window buttons ---------- */
// In the app the host minimizes to the tray or closes (per "Close button hides
// to tray"); in the browser preview, mock.js shows a tray demo.
const inApp = !!(window.chrome && window.chrome.webview);
$("minBtn").addEventListener("click", () => { closeMenu(); inApp ? bridge.send({ cmd: "windowMinimize" }) : window.mockToTray(); });
$("closeBtn").addEventListener("click", () => { closeMenu(); inApp ? bridge.send({ cmd: "windowClose" }) : window.mockToTray(); });
JS
replace(\$js, "/* ---------- presets: shared helpers ---------- */", "$windowButtons\n/* ---------- presets: shared helpers ---------- */");

write_file('app.js', "/* $header */\n$js");

# ---------------------------------------------------------------- mock.js
(my $demoWired = $demoWiring) =~ s/bridge\.send/send/g;
$demoWired =~ s/^/    /mg;
(my $demoRenderBody = $demoRender) =~ s/^  /    /mg;
(my $trayFns = $trayDemo) =~ s/^\$\("(minBtn|closeBtn)"\).*\n//mg;
$trayFns =~ s/function toTray\(\) \{ closeMenu\(\); /function toTray() { /;
$trayFns =~ s/^/  /mg;

my $mock = <<"JS";
/* $header

   Browser preview only: a simulated host with fake devices and audio, the
   "Simulate" buttons and a tray demo, so the UI can be designed and tested
   without the C++ engine. The app never loads this file. */
"use strict";
(() => {
  if (window.chrome && window.chrome.webview) return;  // running inside the app

  const \$ = (id) => document.getElementById(id);
  const setText = (id, txt) => { const el = \$(id); if (el.textContent !== txt) el.textContent = txt; };

  document.addEventListener("DOMContentLoaded", () => {}, { once: true });
  const stage = document.querySelector(".stage");
  stage.insertAdjacentHTML("beforeend", `$note`);
  stage.insertAdjacentHTML("beforeend", `$demo`);
  document.body.insertAdjacentHTML("beforeend", `$tray`);
  setText("aboutNote", "Interactive mockup · simulated audio");

$trayFns
  \$("restoreBtn").addEventListener("click", fromTray);
  window.mockToTray = toTray;

  window.mockRender = (s) => {
$demoRenderBody
  };

  window.createMockHost = () => {
    const host = createMockHost();
    const send = host.send;
$demoWired    return host;
  };

$mockHost
})();
JS
write_file('mock.js', $mock);
print "ui/index.html, app.css, app.js, mock.js written\n";
