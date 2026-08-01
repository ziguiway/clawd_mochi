# gifenc

`data/gif_encoder.js` vendors gifenc 1.0.3 by Matt DesLauriers.

- Source: https://github.com/mattdesl/gifenc
- License: MIT
- Purpose: browser-side GIF re-encoding and size optimization

The bundled CommonJS distribution is wrapped for use as `window.GifEnc` in
the offline device controller. The source map reference is omitted.

Source dimensions are retained when they fit the 240x240 display. Larger GIFs
are normalized once to the native display canvas before encoding.
