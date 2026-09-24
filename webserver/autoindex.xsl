<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:output
    method="html"
    encoding="UTF-8"
    indent="no"
    doctype-system="about:legacy-compat"/>

  <!-- nginx emits <list> as the root element when autoindex_format xml; is set -->
  <xsl:template match="/list">
    <html lang="en">
      <head>
        <meta charset="UTF-8"/>
        <title>Index</title>
        <style>
          body   { font-family: system-ui, sans-serif; margin: 2rem; }
          table  { border-collapse: collapse; width: 100%; }
          th, td { text-align: left; padding: 4px 12px; }
          tr:nth-child(even) { background: #f5f5f5; }
          #custom-banner { margin-bottom: 1.5rem; }
        </style>
      </head>
      <body>
        <script src="https://cdn.jsdelivr.net/npm/hls.js@1"></script>
        <video id="video"></video>
        <script>
          var video = document.getElementById('video');
          var videoSrc = 'https://bergpad.nationalespeeltuin.nl/livestream.m3u8';
          if (Hls.isSupported()) {
            var hls = new Hls();
            hls.loadSource(videoSrc);
            hls.attachMedia(video);
          }
          // HLS.js is not supported on platforms that do not have Media Source
          // Extensions (MSE) enabled.
          //
          // When the browser has built-in HLS support (check using `canPlayType`),
          // we can provide an HLS manifest (i.e. .m3u8 URL) directly to the video
          // element through the `src` property. This is using the built-in support
          // of the plain video element, without using HLS.js.
          else if (video.canPlayType('application/vnd.apple.mpegurl')) {
            video.src = videoSrc;
          }
        </script>

        <table>
          <thead>
            <tr><th>Name</th><th>Last modified</th><th>Size</th></tr>
          </thead>
          <tbody>
            <tr>
              <td><a href="../">../</a></td>
              <td></td>
              <td>-</td>
            </tr>

            <!-- directories: always shown -->
            <xsl:apply-templates select="directory"/>

            <!-- files: hide anything ending in .ts, case-insensitive -->
            <xsl:apply-templates select="file[
              not(
                translate(
                  substring(., string-length(.) - 2),
                  'TS', 'ts'
                ) = '.ts'
              )
            ]"/>
          </tbody>
        </table>
      </body>
    </html>
  </xsl:template>

  <xsl:template match="directory">
    <tr>
      <td><a href="{.}/"><xsl:value-of select="."/>/</a></td>
      <td><xsl:value-of select="@mtime"/></td>
      <td>-</td>
    </tr>
  </xsl:template>

  <xsl:template match="file">
    <tr>
      <td><a href="{.}"><xsl:value-of select="."/></a></td>
      <td><xsl:value-of select="@mtime"/></td>
      <td><xsl:value-of select="@size"/></td>
    </tr>
  </xsl:template>

</xsl:stylesheet>
