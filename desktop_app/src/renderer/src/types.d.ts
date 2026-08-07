export interface DeviceInfo { name: string; ip: string; lastSeen: number; }
export interface StreamState {
  running: boolean; ip: string | null; frames: number;
  mode: "cursor" | "full" | "region"; fps: number; quality: number;
}
export interface DisplayInfo { id: string; label: string; size: { width: number; height: number }; }
export interface KeyboardPetState { running: boolean; ip: string | null; }

declare global {
  interface Window {
    mochi: {
      startStream(opts: {
        ip: string; mode: string; fps: number; quality: number;
        sourceId?: string | null; region?: { x: number; y: number; w: number; h: number } | null;
      }): Promise<void>;
      stopStream(): Promise<void>;
      getStreamState(): Promise<StreamState>;
      getDevices(): Promise<DeviceInfo[]>;
      checkDevice(ip: string): Promise<boolean>;
      listDisplays(): Promise<DisplayInfo[]>;
      checkScreenPermission(): Promise<string>;
      openScreenPermissionSettings(): Promise<void>;
      onStreamState(cb: (s: Partial<StreamState>) => void): void;
      onStreamFrame(cb: (b64: string) => void): void;
      onDevices(cb: (d: DeviceInfo[]) => void): void;
      startKeyboardPet(ip: string): Promise<void>;
      stopKeyboardPet(): Promise<void>;
      getKeyboardPetState(): Promise<KeyboardPetState>;
      onKeyboardPetState(cb: (s: KeyboardPetState) => void): void;
    };
  }
}
export {};
