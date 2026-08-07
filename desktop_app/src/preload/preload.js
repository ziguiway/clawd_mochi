const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("mochi", {
  startStream: (opts) => ipcRenderer.invoke("stream:start", opts),
  stopStream: () => ipcRenderer.invoke("stream:stop"),
  getStreamState: () => ipcRenderer.invoke("stream:getState"),
  getDevices: () => ipcRenderer.invoke("devices:get"),
  checkDevice: (ip) => ipcRenderer.invoke("devices:check", ip),
  listDisplays: () => ipcRenderer.invoke("displays:list"),
  checkScreenPermission: () => ipcRenderer.invoke("screen:checkPermission"),
  openScreenPermissionSettings: () => ipcRenderer.invoke("screen:openPermissionSettings"),
  onStreamState: (cb) => ipcRenderer.on("stream:state", (_e, s) => cb(s)),
  onStreamFrame: (cb) => ipcRenderer.on("stream:frame", (_e, b64) => cb(b64)),
  onDevices: (cb) => ipcRenderer.on("devices:list", (_e, d) => cb(d))
  ,startKeyboardPet: (ip) => ipcRenderer.invoke("pet:start", ip),
  stopKeyboardPet: () => ipcRenderer.invoke("pet:stop"),
  getKeyboardPetState: () => ipcRenderer.invoke("pet:getState"),
  onKeyboardPetState: (cb) => ipcRenderer.on("pet:state", (_e, s) => cb(s))
});
