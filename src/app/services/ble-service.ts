import { Injectable, PLATFORM_ID, Inject, NgZone } from '@angular/core';
import { isPlatformBrowser } from '@angular/common';
import { BehaviorSubject } from 'rxjs';

@Injectable({ 
  providedIn: 'root' 
})
export class BleService {
  public deviceValue$ = new BehaviorSubject<string[]>('0.00','0.00','0.00','0.00');
  private gattServer: BluetoothRemoteGATTServer | null = null;
  private dataCharacteristic: any = null;
  private commandChar: any = null;
  public isConnected$ = new BehaviorSubject<boolean>(false);
  private dataBtye : parseData = {
    temp1 : 1,
    temp2 : 1,
    temp3 : 1,
    current1 : 1,
    current2 : 1,
    current3 : 1,
    volt1 : 1,
    volt2 : 1,
    volt3 : 1,
    faultFlag : 1,
    reservation : 1
  };
  private buf : ArrayBuffer = new ArrayBuffer(100);

  constructor(@Inject(PLATFORM_ID) private platformId: object, private ngZone: NgZone) {}

  // This handles the BLE ESP32 connection logic
  async connect() {
    if (!isPlatformBrowser(this.platformId)) return;

    try {
      const device = await navigator.bluetooth.requestDevice({
        filters: [{ name: 'MyESP32' }],
        optionalServices: ['ab2d02b4-ad53-400f-bf7e-d603a657d07d']
      });

      console.log("Retreived device");

      // Add the disconnect listener
      device.addEventListener('gattserverdisconnected', () => {
        console.warn("ESP32 Disconnected!");
        this.isConnected$.next(false);
        this.gattServer = null;
        this.dataCharacteristic = null;
        this.commandChar = null;
      });
      console.log("Added disconnect");

      this.gattServer = await device.gatt!.connect();
      const service = await this.gattServer.getPrimaryService('ab2d02b4-ad53-400f-bf7e-d603a657d07d');
      console.log("Retreived service");

      const dataChar = await service.getCharacteristic('05ac146f-aee8-4659-abba-882c1f7e0372');
      this.dataCharacteristic = dataChar;
      console.log("Retreived characteristics");

      await dataChar.startNotifications();
      dataChar.addEventListener('characteristicvaluechanged', (event: any) => {
        const dataView = event.target.value as DataView;
  
        this.ngZone.run(() => {
          this.parseDt(dataView);
        });
      });
      device.addEventListener('gattserverdisconnected', () => {
        this.ngZone.run(() => {
          this.isConnected$.next(false);
          this.deviceValue$.next('0.00', '0.00','0.00','0.00'); // Reset value on disconnect
        });
      });
      console.log("Added event listener");


      // 2. Setup the Command Characteristic (to send triggers)
      this.commandChar = await service.getCharacteristic('58bb99f3-75cb-48cb-81e4-346cc4f0687d');
      console.log("Connected and Command Char ready.");

      // If we got this far, we are connected
      this.isConnected$.next(true);
    } catch (error) {
      this.isConnected$.next(false);
      console.error("Connection failed:", error);
      throw error;
    }
  }


  async sendCommand(cmdValue: string) {
    if (!this.commandChar) {
      console.error("Command failed to proceed.");
      return;
    }
  
    // Convert string to Uint8Array for BLE transmission
    const data = new TextEncoder().encode(cmdValue);
  
    try {
      await this.commandChar.writeValueWithResponse(data);
      console.log("Command sent:", cmdValue);
    } catch (error) {
      console.error("Failed to send command:", error);
    }
  }


  /*
  async read(): Promise<string> {
    if (!this.dataCharacteristic) {
      console.error("Not connected to a characteristic yet.");
      //return;
      return '0.00';
    }
  
    try {
      // Manually pull the current value from the ESP32
      const value = await this.dataCharacteristic.readValue();
      const decoded = new TextDecoder().decode(value); // Remove later, but first get accurate data
      console.log("Value read");

      // Also wrap the manual read
      this.ngZone.run(() => {
        this.deviceValue$.next(decoded);
      });
      return decoded; // Update the stream
    } catch (error) {
      console.error("Manual read failed:", error, ", assuming disconnect - please re-pair.");

      this.ngZone.run(() => {
        this.isConnected$.next(false);
      });

      return '-0.01';
    }
  }
    */

  // Custom byte parsing
  parseDt(view: DataView) {
    console.log("Raw Bytes Received:", new Uint8Array(view.buffer));
    if (view.byteLength < 20) {
      // This is likely a FRAME_TYPE_FAULT (which is smaller)
      return;
    }
    try {
      this.dataBtye = {
        temp1 : view.getInt16(0,true),//0
        temp2 : view.getInt16(2,true),//2
        temp3 : view.getInt16(4,true),//4
        current1 : view.getUint16(6,true),//6
        current2 : view.getUint16(8,true),//8
        current3 : view.getUint16(10,true),//10
        volt1 : view.getUint16(12,true),//12
        volt2 : view.getUint16(14,true),//14
        volt3 : view.getUint16(16,true),//16
        faultFlag : view.getUint8(18),//18
        reservation : view.getUint8(19)//20
      }
      const avgTemp = (this.dataBtye.temp1 + this.dataBtye.temp2 + this.dataBtye.temp3) / 3 / 100;
      const groupDt = [
        avgTemp.toFixed(2),
        this.dataBtye.temp1.toFixed(2),
        this.dataBtye.temp2.toFixed(2),
        this.dataBtye.temp3.toFixed(2)
      ]
      this.deviceValue$.next(groupDt);
      console.log("Service parsed new value:", avgTemp);
    } catch (e) {
      console.error(`Parsing error at length ${view.byteLength}:`, e);
    }
  }
}

interface parseData {
  temp1 : number;
  temp2 : number;
  temp3 : number;
  current1 : number;
  current2 : number;
  current3 : number;
  volt1 : number;
  volt2 : number;
  volt3 : number;
  faultFlag : number;
  reservation : number;
}

export interface TempData {
  average: string,
  t1: string,
  t2: string,
  t3: string
}