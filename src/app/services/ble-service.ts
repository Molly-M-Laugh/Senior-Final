import { Injectable, PLATFORM_ID, Inject, NgZone } from '@angular/core';
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
      // Raw bytes don't follow: utf-8, utf-16, utf-16le, utf-16be
      dataChar.addEventListener('characteristicvaluechanged', (event: any) => {

        //const decoder = new TextDecoder('utf-8', { fatal: true });
        try {
          //const dataBytes: Uint8Array = new Uint8Array(event.target.value)
          //const value = decoder.decode(dataBytes);
          this.parseDt(this.buf)
          console.log("Value updated in Zone: ", this.dataBtye);
          //this.ngZone.run(() => {
            //this.deviceValue$.next(value);
            //console.log("Value updated in Zone:", value);
          //});
        } catch (e) {
          console.error("Invalid TextDecoder sequence detected");
        }
      //const value = new TextDecoder('utf-8').decode(event.target.value);

      // Force Angular to recognize this asynchronous Bluetooth event
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


  async read(): Promise<string> {
    if (!this.dataCharacteristic) {
      console.error("Not connected to a characteristic yet.");
      //return;
      return '0.00';
    }
  
    try {
      // Manually pull the current value from the ESP32
      const value = await this.dataCharacteristic.readValue();
      const decoded = new TextDecoder().decode(value);

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

  // Custom byte parsing
  parseDt(buffer:ArrayBuffer) {
    const view = new DataView(buffer)
    this.dataBtye = {
      temp1 : view.getInt16(0,true),
      temp2 : view.getInt16(2,true),
      temp3 : view.getInt16(4,true),
      current1 : view.getUint16(6,true),
      current2 : view.getUint16(8,true),
      current3 : view.getUint16(10,true),
      volt1 : view.getUint8(12),
      volt2 : view.getUint8(14),
      volt3 : view.getUint8(16),
      faultFlag : view.getUint8(18),
      reservation : view.getUint8(20)
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