(() => {
  'use strict';

  const WIDTH = 240;
  const HEIGHT = 240;
  const STREAM_FPS = 6;
  const FRAME_INTERVAL_MS = 1000 / STREAM_FPS;
  const MAX_FILE_BYTES = 100 * 1024 * 1024;

  let selectedFile = null;
  let sourceElement = null;
  let objectUrl = '';
  let sourceKind = '';
  let casting = false;
  let deviceActive = false;
  let castGeneration = 0;
  let lastFrame = null;
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
    byId('mediaPlay').disabled = !selectedFile || casting;
    byId('mediaStop').disabled = !casting && !deviceActive;
    byId('mediaFit').disabled = casting;
    byId('mediaBg').disabled = casting;
  }

  function releaseSource() {
    if (sourceElement instanceof HTMLVideoElement) {
      sourceElement.pause();
      sourceElement.removeAttribute('src');
      sourceElement.load();
    }
    sourceElement = null;
    if (objectUrl) URL.revokeObjectURL(objectUrl);
    objectUrl = '';
    gifReader = null;
    gifPixels = null;
    gifPreviousInfo = null;
    gifRestorePixels = null;
  }

  function sourceSize() {
    if (sourceElement instanceof HTMLVideoElement) {
      return [sourceElement.videoWidth, sourceElement.videoHeight];
    }
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

  function dirtyRegion(frame) {
    if (!lastFrame) return {x: 0, y: 0, width: WIDTH, height: HEIGHT};
    let left = WIDTH;
    let top = HEIGHT;
    let right = -1;
    let bottom = -1;
    for (let y = 0; y < HEIGHT; y++) {
      const row = y * WIDTH;
      for (let x = 0; x < WIDTH; x++) {
        const index = row + x;
        if (frame[index] === lastFrame[index]) continue;
        if (x < left) left = x;
        if (x > right) right = x;
        if (y < top) top = y;
        if (y > bottom) bottom = y;
      }
    }
    if (right < left) return null;
    return {
      x: left,
      y: top,
      width: right - left + 1,
      height: bottom - top + 1,
    };
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

  async function sendCurrentFrame(generation) {
    if (generation !== castGeneration) return false;
    const frame = currentRgb565Frame();
    const region = dirtyRegion(frame);
    if (!region) return true;
    const form = new FormData();
    form.append('frame', new Blob([encodeRegion(frame, region)], {
      type: 'application/octet-stream',
    }), 'frame.rgb565');
    const query = new URLSearchParams({
      x: region.x,
      y: region.y,
      w: region.width,
      h: region.height,
    });
    const response = await fetch(`/media/frame?${query}`, {
      method: 'POST',
      body: form,
      cache: 'no-store',
    });
    let payload = {};
    try { payload = await response.json(); } catch (_) {}
    if (!response.ok) {
      throw new Error(payload.error || 'frame upload failed');
    }
    lastFrame = frame;
    deviceActive = true;
    setControls();
    return generation === castGeneration;
  }

  function wait(milliseconds) {
    return new Promise(resolve => setTimeout(resolve, milliseconds));
  }

  async function streamAnimatedSource(generation) {
    if (sourceKind === 'gif') {
      await streamDecodedGif(generation);
      return;
    }
    if (sourceKind === 'video') {
      sourceElement.currentTime = 0;
      sourceElement.muted = true;
      sourceElement.playsInline = true;
      await sourceElement.play();
    }

    while (casting && generation === castGeneration) {
      if (sourceKind === 'video' && sourceElement.ended) break;
      const started = performance.now();
      drawSource();
      await sendCurrentFrame(generation);
      const remaining = FRAME_INTERVAL_MS - (performance.now() - started);
      if (remaining > 0) await wait(remaining);
    }

    if (generation !== castGeneration) return;
    casting = false;
    if (sourceKind === 'video') sourceElement.pause();
    setState(sourceKind === 'video' ? 'VIDEO ENDED' : 'DISPLAYED');
    setControls();
  }

  async function streamDecodedGif(generation) {
    const loopCount = gifReader.loopCount();
    const totalPlays = loopCount === 0 ? Infinity
      : loopCount === null ? 1 : loopCount + 1;
    let play = 0;
    while (casting && generation === castGeneration && play < totalPlays) {
      resetGifComposition();
      for (let index = 0; index < gifReader.numFrames(); index++) {
        if (!casting || generation !== castGeneration) return;
        const started = performance.now();
        const info = decodeGifFrame(index);
        drawGifPixels();
        await sendCurrentFrame(generation);
        const frameDelay = Math.min(10_000,
          Math.max(20, info.delay > 0 ? info.delay * 10 : 100));
        const remaining = frameDelay - (performance.now() - started);
        if (remaining > 0) await wait(remaining);
      }
      play++;
    }
    if (generation !== castGeneration) return;
    casting = false;
    setState('GIF ENDED');
    setControls();
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
    if (file.size > MAX_FILE_BYTES) {
      setState('FILE TOO LARGE', true);
      toast('media is limited to 100 MB', false);
      return;
    }

    await window.stopMediaCast(true);
    releaseSource();
    selectedFile = file;
    objectUrl = URL.createObjectURL(file);
    sourceKind = file.type.startsWith('video/') ? 'video'
      : file.type === 'image/gif' ? 'gif' : 'image';
    setState('DECODING');

    try {
      if (sourceKind === 'video') {
        const video = document.createElement('video');
        video.preload = 'auto';
        video.muted = true;
        video.playsInline = true;
        video.src = objectUrl;
        await new Promise((resolve, reject) => {
          video.addEventListener('loadeddata', resolve, {once: true});
          video.addEventListener('error', () => reject(
            new Error('browser cannot decode this video')), {once: true});
        });
        sourceElement = video;
      } else {
        const image = new Image();
        image.src = objectUrl;
        if (image.decode) await image.decode();
        else await new Promise((resolve, reject) => {
          image.onload = resolve;
          image.onerror = () => reject(new Error('image decode failed'));
        });
        sourceElement = image;
      }
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
      if (sourceKind === 'image') {
        await sendCurrentFrame(generation);
        casting = false;
        setState('DISPLAYED');
        setControls();
        toast('image displayed');
      } else {
        await streamAnimatedSource(generation);
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
    if (sourceElement instanceof HTMLVideoElement) sourceElement.pause();
    if (shouldNotifyDevice) {
      try {
        await fetch('/media/stop', {method: 'POST', cache: 'no-store'});
      } catch (_) {}
    }
    deviceActive = false;
    lastFrame = null;
    setState(selectedFile ? `${sourceKind.toUpperCase()} READY` : 'CHOOSE FILE');
    setControls();
    if (!silent && shouldNotifyDevice) toast('media stopped');
  };

  window.addEventListener('beforeunload', () => {
    if (deviceActive || casting) {
      navigator.sendBeacon('/media/stop', new Blob([], {type: 'text/plain'}));
    }
    releaseSource();
  });
})();
