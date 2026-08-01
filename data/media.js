(() => {
  'use strict';

  const WIDTH = 240;
  const HEIGHT = 240;
  const MAX_FILE_BYTES = 100 * 1024 * 1024;
  const MAX_ANIMATION_BYTES = 560 * 1024;

  let selectedFile = null;
  let sourceElement = null;
  let objectUrl = '';
  let sourceKind = '';
  let casting = false;
  let deviceActive = false;
  let castGeneration = 0;
  let gifReader = null;
  let gifPixels = null;
  let gifPreviousInfo = null;
  let gifRestorePixels = null;
  const gifCanvas = document.createElement('canvas');
  const gifContext = gifCanvas.getContext('2d');

  const byId = id => document.getElementById(id);
  const canvas = () => byId('mediaPreview');
  const context = () => canvas().getContext('2d', {
    alpha: false,
    willReadFrequently: true,
  });

  function setState(value, error = false) {
    const state = byId('mediaState');
    state.textContent = value;
    state.style.color = error ? '#ff7b6b' : '';
    state.style.borderColor = error ? '#7a302b' : '';
  }

  function setControls() {
    byId('mediaPlay').disabled = !selectedFile || casting || deviceActive;
    byId('mediaStop').disabled = !casting && !deviceActive;
    byId('mediaFit').disabled = casting;
    byId('mediaBg').disabled = casting;
  }

  function releaseSource() {
    sourceElement = null;
    if (objectUrl) URL.revokeObjectURL(objectUrl);
    objectUrl = '';
    gifReader = null;
    gifPixels = null;
    gifPreviousInfo = null;
    gifRestorePixels = null;
  }

  function sourceSize() {
    return [sourceElement?.naturalWidth || 0, sourceElement?.naturalHeight || 0];
  }

  function drawSource() {
    const ctx = context();
    const bg = byId('mediaBg').value || '#000000';
    ctx.fillStyle = bg;
    ctx.fillRect(0, 0, WIDTH, HEIGHT);
    if (!sourceElement) return;

    const [sourceWidth, sourceHeight] = sourceSize();
    if (!sourceWidth || !sourceHeight) return;
    const fit = byId('mediaFit').value;
    let x = 0;
    let y = 0;
    let width = WIDTH;
    let height = HEIGHT;
    if (fit !== 'stretch') {
      const scale = fit === 'contain'
        ? Math.min(WIDTH / sourceWidth, HEIGHT / sourceHeight)
        : Math.max(WIDTH / sourceWidth, HEIGHT / sourceHeight);
      width = sourceWidth * scale;
      height = sourceHeight * scale;
      x = (WIDTH - width) / 2;
      y = (HEIGHT - height) / 2;
    }
    ctx.imageSmoothingEnabled = true;
    ctx.imageSmoothingQuality = 'high';
    ctx.drawImage(sourceElement, x, y, width, height);
  }

  function clearGifRect(info) {
    if (!gifPixels || !info) return;
    for (let y = info.y; y < info.y + info.height; y++) {
      const start = (y * gifReader.width + info.x) * 4;
      gifPixels.fill(0, start, start + info.width * 4);
    }
  }

  function resetGifComposition() {
    if (!gifReader) return;
    gifPixels = new Uint8ClampedArray(
      gifReader.width * gifReader.height * 4
    );
    gifPreviousInfo = null;
    gifRestorePixels = null;
  }

  function decodeGifFrame(index) {
    if (gifPreviousInfo?.disposal === 2) {
      clearGifRect(gifPreviousInfo);
    } else if (gifPreviousInfo?.disposal === 3 && gifRestorePixels) {
      gifPixels.set(gifRestorePixels);
    }
    const info = gifReader.frameInfo(index);
    const restore = info.disposal === 3 ? gifPixels.slice() : null;
    gifReader.decodeAndBlitFrameRGBA(index, gifPixels);
    gifPreviousInfo = info;
    gifRestorePixels = restore;
    return info;
  }

  function drawGifPixels() {
    gifCanvas.width = gifReader.width;
    gifCanvas.height = gifReader.height;
    gifContext.putImageData(new ImageData(
      gifPixels, gifReader.width, gifReader.height
    ), 0, 0);

    const ctx = context();
    ctx.fillStyle = byId('mediaBg').value || '#000000';
    ctx.fillRect(0, 0, WIDTH, HEIGHT);
    const fit = byId('mediaFit').value;
    let x = 0;
    let y = 0;
    let width = WIDTH;
    let height = HEIGHT;
    if (fit !== 'stretch') {
      const scale = fit === 'contain'
        ? Math.min(WIDTH / gifReader.width, HEIGHT / gifReader.height)
        : Math.max(WIDTH / gifReader.width, HEIGHT / gifReader.height);
      width = gifReader.width * scale;
      height = gifReader.height * scale;
      x = (WIDTH - width) / 2;
      y = (HEIGHT - height) / 2;
    }
    ctx.imageSmoothingEnabled = true;
    ctx.imageSmoothingQuality = 'high';
    ctx.drawImage(gifCanvas, x, y, width, height);
  }

  function currentRgb565Frame() {
    const rgba = context().getImageData(0, 0, WIDTH, HEIGHT).data;
    const output = new Uint16Array(WIDTH * HEIGHT);
    for (let source = 0, target = 0; source < rgba.length;
         source += 4, target++) {
      output[target] = ((rgba[source] & 0xf8) << 8) |
                       ((rgba[source + 1] & 0xfc) << 3) |
                       (rgba[source + 2] >> 3);
    }
    return output;
  }

  function encodeRegion(frame, region) {
    const output = new Uint8Array(region.width * region.height * 2);
    let target = 0;
    for (let y = region.y; y < region.y + region.height; y++) {
      let source = y * WIDTH + region.x;
      for (let x = 0; x < region.width; x++, source++, target += 2) {
        const value = frame[source];
        output[target] = value >> 8;
        output[target + 1] = value & 0xff;
      }
    }
    return output;
  }

  async function buildOptimizedGif() {
    if (!window.GifEnc) return selectedFile;
    const {GIFEncoder, quantize, applyPalette} = window.GifEnc;
    const encoder = GIFEncoder();
    const normalizeForDisplay = gifReader.width > WIDTH ||
      gifReader.height > HEIGHT;
    // Partial transparent frames rely on disposal semantics. Flatten these
    // into opaque composited frames in the browser so the device never has to
    // clear a large previous frame while it is actively refreshing the TFT.
    const flattenFrames = normalizeForDisplay ||
      Array.from({length: gifReader.numFrames()}, (_, index) =>
        gifReader.frameInfo(index)).some(info =>
          info.transparent_index !== null || info.disposal === 2 ||
          info.disposal === 3 ||
          info.x !== 0 || info.y !== 0 ||
          info.width !== gifReader.width || info.height !== gifReader.height);
    const outputWidth = normalizeForDisplay ? WIDTH : gifReader.width;
    const outputHeight = normalizeForDisplay ? HEIGHT : gifReader.height;
    const repeat = gifReader.loopCount() === null
      ? -1 : gifReader.loopCount();
    resetGifComposition();
    for (let index = 0; index < gifReader.numFrames(); index++) {
      const info = decodeGifFrame(index);
      let pixels = gifPixels;
      if (flattenFrames) {
        if (normalizeForDisplay) {
          drawGifPixels();
          pixels = context().getImageData(
            0, 0, outputWidth, outputHeight).data;
        } else {
          gifCanvas.width = gifReader.width;
          gifCanvas.height = gifReader.height;
          gifContext.clearRect(0, 0, outputWidth, outputHeight);
          gifContext.putImageData(
            new ImageData(gifPixels, gifReader.width, gifReader.height), 0, 0);
          gifContext.globalCompositeOperation = 'destination-over';
          gifContext.fillStyle = byId('mediaBg').value || '#000000';
          gifContext.fillRect(0, 0, outputWidth, outputHeight);
          gifContext.globalCompositeOperation = 'source-over';
          pixels = gifContext.getImageData(
            0, 0, outputWidth, outputHeight).data;
        }
      }
      const palette = quantize(pixels, 256, {format: 'rgb565'});
      const indexed = applyPalette(pixels, palette, 'rgb565');
      encoder.writeFrame(indexed, outputWidth, outputHeight, {
        palette,
        delay: Math.min(10_000,
          Math.max(20, info.delay > 0 ? info.delay * 10 : 100)),
        repeat,
        dispose: 1,
      });
      if ((index & 3) === 3) await wait(0);
    }
    encoder.finish();
    const optimized = new Blob([encoder.bytes()], {type: 'image/gif'});
    return flattenFrames || optimized.size < selectedFile.size
      ? optimized : selectedFile;
  }

  async function uploadAnimation(animation, generation) {
    if (generation !== castGeneration) return false;
    const form = new FormData();
    form.append('animation', animation, 'animation.gif');
    const response = await fetch('/media/animation', {
      method: 'POST', body: form, cache: 'no-store',
    });
    let payload = {};
    try { payload = await response.json(); } catch (_) {}
    if (!response.ok) {
      throw new Error(payload.error || 'GIF upload failed');
    }
    deviceActive = true;
    return generation === castGeneration;
  }

  async function uploadGifAnimation(generation) {
    setState('OPTIMIZING GIF');
    const animation = await buildOptimizedGif();
    if (animation.size > MAX_ANIMATION_BYTES) {
      throw new Error('GIF exceeds the 560 KB device limit after optimization');
    }
    setState('UPLOADING GIF');
    return uploadAnimation(animation, generation);
  }

  async function sendStaticFrame(generation) {
    const frame = currentRgb565Frame();
    const region = {x: 0, y: 0, width: WIDTH, height: HEIGHT};
    const form = new FormData();
    form.append('frame', new Blob([encodeRegion(frame, region)], {
      type: 'application/octet-stream',
    }), 'frame.rgb565');
    const response = await fetch('/media/frame?x=0&y=0&w=240&h=240', {
      method: 'POST', body: form, cache: 'no-store',
    });
    if (!response.ok) throw new Error('image upload failed');
    deviceActive = true;
    return generation === castGeneration;
  }

  function wait(milliseconds) {
    return new Promise(resolve => setTimeout(resolve, milliseconds));
  }

  window.openMediaPanel = function openMediaPanel() {
    if (typeof isBusy !== 'undefined' &&
        (isBusy || termOpen || canvasOpen)) return;
    activeView = 19;
    document.querySelectorAll('.vbtn').forEach(button => {
      button.classList.toggle('active', Number(button.dataset.v) === 19);
    });
    ['pwrap', 'ywrap', 'mwrap', 'swrap', 'uwrap'].forEach(id => {
      byId(id)?.classList.remove('open');
    });
    byId('mediaWrap').classList.add('open');
    setControls();
    if (!selectedFile) setState('CHOOSE FILE');
  };

  window.closeMediaPanel = async function closeMediaPanel() {
    await window.stopMediaCast(true);
    byId('mediaWrap').classList.remove('open');
    document.querySelectorAll('.vbtn').forEach(button => {
      button.classList.remove('active');
    });
    activeView = 0;
  };

  window.leaveMediaForView = async function leaveMediaForView() {
    if (activeView !== 19) return;
    await window.stopMediaCast(true);
    byId('mediaWrap').classList.remove('open');
  };

  window.selectMediaFile = async function selectMediaFile(file) {
    if (!file) return;
    if (file.type.startsWith('video/')) {
      setState('VIDEO COMING LATER', true);
      toast('video support is deferred until GIF playback is validated', false);
      byId('mediaFile').value = '';
      return;
    }
    if (file.size > MAX_FILE_BYTES) {
      setState('FILE TOO LARGE', true);
      toast('media is limited to 100 MB', false);
      return;
    }

    await window.stopMediaCast(true);
    releaseSource();
    selectedFile = file;
    objectUrl = URL.createObjectURL(file);
    sourceKind = file.type === 'image/gif' ? 'gif' : 'image';
    setState('DECODING');

    try {
      const image = new Image();
      image.src = objectUrl;
      if (image.decode) await image.decode();
      else await new Promise((resolve, reject) => {
        image.onload = resolve;
        image.onerror = () => reject(new Error('image decode failed'));
      });
      sourceElement = image;
      if (sourceKind === 'gif') {
        if (typeof GifReader !== 'function') {
          throw new Error('GIF decoder unavailable');
        }
        const bytes = new Uint8Array(await file.arrayBuffer());
        gifReader = new GifReader(bytes);
        if (gifReader.numFrames() < 1 ||
            gifReader.width * gifReader.height > 4 * 1024 * 1024 ||
            gifReader.numFrames() > 1000) {
          throw new Error('GIF dimensions or frame count exceed limits');
        }
        resetGifComposition();
        decodeGifFrame(0);
        drawGifPixels();
      } else {
        drawSource();
      }
      setState(`${sourceKind.toUpperCase()} READY`);
      setControls();
      toast(`${file.name} ready`);
    } catch (error) {
      selectedFile = null;
      releaseSource();
      setState('UNSUPPORTED', true);
      setControls();
      toast(error.message || 'unsupported media', false);
    } finally {
      byId('mediaFile').value = '';
    }
  };

  window.redrawSelectedMedia = function redrawSelectedMedia() {
    if (!casting) {
      if (sourceKind === 'gif' && gifReader) drawGifPixels();
      else drawSource();
    }
  };

  window.castSelectedMedia = async function castSelectedMedia() {
    if (!selectedFile || !sourceElement || casting) return;
    casting = true;
    const generation = ++castGeneration;
    setState(sourceKind === 'image' ? 'UPLOADING' : 'STREAMING');
    setControls();
    try {
      if (sourceKind === 'gif') drawGifPixels();
      else drawSource();
      if (sourceKind === 'gif') {
        if (await uploadGifAnimation(generation)) {
          casting = false;
          setState('GIF PLAYING');
          setControls();
          toast('GIF uploaded and playing locally');
        }
      } else if (sourceKind === 'image') {
        await sendStaticFrame(generation);
        casting = false;
        setState('DISPLAYED');
        setControls();
        toast('image displayed');
      }
    } catch (error) {
      if (generation !== castGeneration) return;
      casting = false;
      setState('CAST FAILED', true);
      setControls();
      toast(error.message || 'media cast failed', false);
    }
  };

  window.stopMediaCast = async function stopMediaCast(silent = false) {
    const shouldNotifyDevice = deviceActive || casting;
    castGeneration++;
    casting = false;
    if (shouldNotifyDevice) {
      try {
        await fetch('/media/stop', {method: 'POST', cache: 'no-store'});
      } catch (_) {}
    }
    deviceActive = false;
    setState(selectedFile ? `${sourceKind.toUpperCase()} READY` : 'CHOOSE FILE');
    setControls();
    if (!silent && shouldNotifyDevice) toast('media stopped');
  };

  window.addEventListener('beforeunload', () => {
    // 静态图和 GIF 已完整存入设备，页面退出后可继续显示。
    releaseSource();
  });
})();
