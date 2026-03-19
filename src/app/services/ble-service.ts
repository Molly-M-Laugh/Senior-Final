import { Injectable, PLATFORM_ID, Inject } from '@angular/core';
import { isPlatformBrowser } from '@angular/common';
import { BehaviorSubject } from 'rxjs';

@Injectable({ 
  providedIn: 'root' 
})
export class BleService {
  public deviceValue$ = new BehaviorSubject<string>('0.00');
  private gattServer: BluetoothRemoteGATTServer | null = null;
  private dataCharacteristic: any = null;
  private commandChar: any = null;

  constructor(@Inject(PLATFORM_ID) private platformId: object) {}

  async connect() {
    if (!isPlatformBrowser(this.platformId)) return;

    try {
      const device = await navigator.bluetooth.requestDevice({
        filters: [{ name: 'MyESP32' }],
        optionalServices: ['ab2d02b4-ad53-400f-bf7e-d603a657d07d']
      });

      console.log("Retreived device");

      this.gattServer = await device.gatt!.connect();
      const service = await this.gattServer.getPrimaryService('ab2d02b4-ad53-400f-bf7e-d603a657d07d');
      console.log("Retreived service");

      // 1. Setup the Data Characteristic (to receive values)
      /*
      const characteristic = await service.getCharacteristic('05ac146f-aee8-4659-aba5-882c1f7e0372');
      this.dataCharacteristic = characteristic;

      // Start notifications so we don't have to poll manually
      await characteristic.startNotifications();
      characteristic.addEventListener('characteristicvaluechanged', (event: any) => {
        const value = new TextDecoder().decode(event.target.value);
        this.deviceValue$.next(value);
      });

      console.log("Connected to ESP32 via Browser");
      */
    const dataChar = await service.getCharacteristic('05ac146f-aee8-4659-aba5-882c1f7e0372');
    this.dataCharacteristic = dataChar;
    console.log("Retreived characteristic");
    await dataChar.startNotifications();
    dataChar.addEventListener('characteristicvaluechanged', (event: any) => {
      const value = new TextDecoder().decode(event.target.value);
      this.deviceValue$.next(value);
    });
    console.log("Added event listener");

    // 2. Setup the Command Characteristic (to send triggers)
    this.commandChar = await service.getCharacteristic('58bb99f3-75cb-48cb-81e4-346cc4f0687d');

    console.log("Connected and Command Char ready.");
    } catch (error) {
      console.error("Connection failed:", error);
    }
  }

  async sendCommand(cmdValue: string) {
    if (!this.commandChar) return;
  
    // Convert string to Uint8Array for BLE transmission
    const encoder = new TextEncoder();
    const data = encoder.encode(cmdValue);
  
    try {
      await this.commandChar.writeValue(data);
      console.log("Command sent:", cmdValue);
    } catch (error) {
      console.error("Failed to send command:", error);
    }
  }

  async read() {
    if (!this.dataCharacteristic) {
      console.error("Not connected to a characteristic yet.");
      return;
    }
  
    try {
      // Manually pull the current value from the ESP32
      const value = await this.dataCharacteristic.readValue();
      const decoded = new TextDecoder().decode(value);
      this.deviceValue$.next(decoded); // Update the stream
    } catch (error) {
      console.error("Manual read failed:", error);
    }
  }
}