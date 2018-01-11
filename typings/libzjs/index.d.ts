// Copyright (c) 2016-2017, Intel Corporation.

// Events
interface EventEmitter {
    on(event: string, listener: (...optional: any[]) => void): void;
}

// AIO
interface AIO {
    open(init: AIOInit): AIOPin;
}

interface AIOInit {
    pin: number;
}

interface AIOPin extends EventEmitter {
    read(): number;
    readAsync(callback: (value: number) => void): void;
    close(): void;
}

// Arduino 101 Pins
interface Arduino101Pins {
    IO2: number;
    IO3: number;
    IO4: number;
    IO5: number;
    IO6: number;
    IO7: number;
    IO8: number;
    IO9: number;
    I10: number;
    I11: number;
    I12: number;
    I13: number;
    LED0: number;
    LED1: number;
    LED2: number;
    PWM0: number;
    PWM1: number;
    PWM2: number;
    PWM3: number;
    A0: number;
    A1: number;
    A2: number;
    A3: number;
    A4: number;
    A5: number;
}

// BLE
interface BLE extends EventEmitter {
    disconnect(address: string): void;
    startAdvertising(name: string, uuid: string[], url: string): void;
    stopAdvertising(): void;
    setServices(services: PrimaryService[]): void;
    PrimaryService: PrimaryServiceConstructor;
    Characteristic: CharacteristicConstructor;
    Descriptor: DescriptorConstructor;
}

interface PrimaryServiceConstructor {
    new(init: PrimaryServiceInit): PrimaryService;
    readonly prototype: PrimaryService;
}

interface CharacteristicConstructor {
    new(init: CharacteristicInit): Characteristic;
    readonly prototype: Characteristic;
}

interface DescriptorConstructor {
    new(init: DescriptorInit): Descriptor;
    readonly prototype: Descriptor;
}

interface PrimaryServiceInit {
    uuid: string;
    characteristics: Characteristic[];
}

interface CharacteristicInit {
    uuid: string;
    properties: string[];
    descriptors: Descriptor[];
    onReadRequest?: (offset: number, callback: (result: number, data: Buffer) => void) => void;
    onWriteRequest?: (data: Buffer, offset: number, withoutResponse: boolean, callback: (result: number) => void) => void;
    onSubscribe?: (result: number, data: Buffer) => void;
    onUnsubscribe?: () => void;
    onNotify?: () => void;
}

interface DescriptorInit {
    uuid: string;
    value: string;
}

interface PrimaryService {
    uuid: string;
    characteristics: Characteristic[];
}

interface Characteristic {
    onReadRequest: (offset: number, callback: (result: number, data: Buffer) => void) => void;
    onWriteRequest: (data: Buffer, offset: number, withoutResponse: boolean, callback: (result: number) => void) => void;
    onSubscribe: (result: number, data: Buffer) => void;
    onUnsubscribe: () => void;
    onNotify: () => void;
    RESULT_SUCCESS: number;
    RESULT_INVALID_OFFSET: number;
    RESULT_INVALID_ATTRIBUTE_LENGTH: number;
    RESULT_UNLIKELY_ERROR: number;
}

interface Descriptor {
    uuid: string;
    value: string;
}

// Board
interface Board {
    name: string;
    version: string;
}

// Buffer
interface Buffer {
    readUInt8(offset: number): number;
    writeUInt8(value: number, offset: number): number;
    readUInt16BE(offset: number): number;
    writeUInt16BE(value: number, offset: number): number;
    readUInt16LE(offset: number): number;
    writeUInt16LE(value: number, offset: number): number;
    readUInt32BE(offset: number): number;
    writeUInt32BE(value: number, offset: number): number;
    readUInt32LE(offset: number): number;
    writeUInt32LE(value: number, offset: number): number;
    toString(encoding: string): string;
    fill(value:number, offset: number, end: number, encoding: string): Buffer;
    fill(value:Buffer, offset: number, end: number, encoding: string): Buffer;
    fill(value:String, offset: number, end: number, encoding: string): Buffer;
}

interface BufferConstructor {
    new(length?: number): Buffer;
    readonly prototype: Buffer;
}

declare const Buffer: BufferConstructor

// Console
declare class console {
    static assert(value: boolean, message?: string): void;
    static error(message?: string): void;
    static warn(message?: string): void;
    static log(message?: string): void;
    static info(message?: string): void;
    static time(label: string): void;
    static timeEnd(label: string): void;
}

// Dgram (UDP sockets)
interface Dgram {
    createSocket(udp4_or_udp6: string): DgramSocket;
}

interface DgramSocket extends EventEmitter {
    on(event: string, onreceive: (msg: Buffer, rinfo: DgramRemoteInfo) => void): void;
    bind(port: number, ip_addr: string): void;
    send(buf: Buffer, offset: number, len: number, port: number, ip_addr: string, onsend: (error: Error) => void): void;
    close(): void;
}

interface DgramRemoteInfo {
    ip_addr: string;
    family: string;
    port: number;
}

// FileSystem
interface Stat {
    isFile(): boolean;
    isDirectory(): boolean;
}

interface FS {
    openSync(path: string, mode: string): Object;
    closeSync(fs: Object): void;
    unlinkSync(path: string): void;
    rmDirSync(path: string): void;
    writeSync(fd: Object, data: Buffer, offset: number, length: number, position?: number): number;
    readSync(fd: Object, data: Buffer, offset: number, length: number, position: number): number;
    truncateSync(path: string, length: number): void;
    mkDirSync(path: string): void;
    readDirSync(path: string): string[];
    statSync(path: string): Stat;
    writeFileSync(file: string, data: string | Buffer): Stat;
}

// GPIO
interface GPIO {
    open(init: number | string | GPIOInit): GPIOPin;
}

interface GPIOInit {
    pin: number | string;
    activeLow: boolean;
    mode: string;
    edge: string;
    state: string;
}

interface GPIOPin {
    read(): number;
    write(value: number): void;
    close(): void;
    onchange: (event: GPIOEvent) => void;
}

interface GPIOEvent {
    value: number;
}

// GroveLCD
interface GroveLCD {
    init(): GroveLCDDevice;
    const GLCD_FS_8BIT_MODE: number;
    const GLCD_FS_ROWS_2: number;
    const GLCD_FS_ROWS_1: number;
    const GLCD_FS_DOT_SIZE_BIG: number;
    const GLCD_FS_DOT_SIZE_LITTLE: number;
    const GLCD_DS_DISPLAY_ON: number;
    const GLCD_DS_DISPLAY_OFF: number;
    const GLCD_DS_CURSOR_ON: number;
    const GLCD_DS_CURSOR_OFF: number;
    const GLCD_DS_BLINK_ON: number;
    const GLCD_DS_BLINK_OFF: number;
    const GLCD_IS_SHIFT_INCREMENT: number;
    const GLCD_IS_SHIFT_DECREMENT: number;
    const GLCD_IS_ENTRY_LEFT: number;
    const GLCD_IS_ENTRY_RIGHT: number;
    const GROVE_RGB_WHITE: number;
    const GROVE_RGB_RED: number;
    const GROVE_RGB_GREEN: number;
    const GROVE_RGB_BLUE: number;
}

interface GroveLCDDevice {
    print(text: string): void;
    clear(): void;
    setCursorPos(col: number, row: number): void;
    selectColor(index: number): void;
    setColor(r: number, g: number, b: number): void;
    setFunction(config: number): void;
    getFunction(): number;
    setDisplayState(config: number): void;
    getDisplayState(): number;
}

// I2C
interface I2C {
    open(init: I2CInit): I2CBus;
}

interface I2CInit {
    bus: number;
    speed: number;
}

interface I2CBus {
    write(device: number, data: Buffer): void;
    read(device: number, size: number, registerAddress: number): void;
    burstRead(device: number, size: number, registerAddress: number): void;
}

// K64FPins
interface K64FPins {
    D0: number;
    D1: number;
    D2: number;
    D3: number;
    D4: number;
    D5: number;
    D6: number;
    D7: number;
    D8: number;
    D9: number;
    D10: number;
    D11: number;
    D12: number;
    D13: number;
    D14: number;
    D15: number;
    LEDR: number;
    LEDG: number;
    LEDB: number;
    SW2: number;
    SW3: number;
    PWM0: number;
    PWM1: number;
    PWM2: number;
    PWM3: number;
    PWM4: number;
    PWM5: number;
    PWM6: number;
    PWM7: number;
    PWM8: number;
    PWM9: number;
    A0: number;
    A1: number;
    A2: number;
    A3: number;
    A4: number;
    A5: number;
}

// Net (TCP sockets)
interface Net {
    createServer(onconnection: () => void): NetServer;
    Socket(): NetSocket;
    isIP(input: string): number;
    isIPv4(input: string): boolean;
    isIPv6(input: string): boolean;
}

interface NetSocket extends EventEmitter {
    connect(options: NetAddressOptions, onconnect: () => void): void;
    pause(): void;
    resume(): void;
    setTimeout(timeout: number, ontimeout: () => void): void;
    write(buf: Buffer, onwritedone: () => void): void;
    bufferSize: number;
    bytesRead: number;
    bytesWritten: number;
    localAddress: string;
    localPort: number;
    remoteAddress: string;
    remoteFamily: string;
    remotePort: number;
}

interface NetServer extends EventEmitter {
    address(): NetAddressInfo;
    close(): void;
    getConnections(callback: (error: Error, count: number) => void): void;
    listen(options: object, onlistening: () => void): void;
    listening: boolean;
    maxConnections: number;
}

interface NetAddressOptions {
    port: number;
    host: string;
    localAddress: string;
    localPort: number;
    family: number;
}

interface NetAddressInfo {
    port: number;
    family: string;
    address: string;
}

// NetConfig
interface NetConfig extends EventEmitter {
    setStaticIP(ip: string): boolean;
    dhcp(callback: (address: string, subnet: string, gateway: string) => void): void;
    setBleAddress(address: string): void;
}

// OCF
interface OCFObject {
    server: OCFServer;
    client: OCFClient;
    platform: OCFPlatform;
    device: OCFDevice;
}

interface OCFPlatform {
    id: string;
    osVersion: string;
    model: string;
    manufacturerName: string;
    manufacturerURL: string;
    manufacturerDate: string;
    platformVersion: string;
    firmwareVersion: string;
    supportURL: string;
}

interface OCFDevice {
    uuid: string;
    name: string;
    dataModels: string;
    coreSpecVersion: string;
}

interface OCFServer extends EventEmitter {
    register(init: ResourceInit): Promise<OCFResource>;
}

interface ResourceInit {
    resourcePath: string;
    resourceTypes: string[];
    interfaces: string[];
    discoverable: boolean;
    observable: boolean;
    secure: boolean;
    slow: boolean;
    properties: object;
}

interface OCFClient extends EventEmitter {
    findResources(options: string, listener?: (resource: OCFResource) => void): Promise<OCFResource>;
    retrieve(deviceId: string, options: object): Promise<OCFResource>;
    update(resource: OCFResource): Promise<OCFResource>;
    getPlatformInfo(deviceId: string): Promise<OCFResource>;
    getDeviceInfo(deviceId: string): Promise<OCFResource>;
}

interface OCFClientOptions {
    deviceId: string;
    resourceType: string;
    resourcePath: string;
}

interface OCFResource {
    resourcePath: string;
    properties: object;
}

interface OCFRequest {
    target: OCFResource;
    source: OCFResource;
    data: object;
    respond(data: object): Promise<void>;
}


// Performance
interface Performance {
    now(): number;
}

// PME
interface PME {
    begin(): void;
    forget(): void;
    configure(context: number, classificationMode: number, distanceMode: number, minInfluence: number, maxInfluence: number): void;
    learn(pattern: number[], category: number): void;
    classify(pattern: number[]): number;
    readNeuron(id: number): Neuron;
    writeVector(pattern: number[]): void;
    getCommittedCount(): number;
    getGlobalContext(): number;
    getClassifierMode(): number;
    setClassifierMode(mode: number): void;
    etDistanceMode(): number;
    setDistanceMode(mode: number): void;
    saveNeurons(): JSON[];
    restoreNeurons(object: JSON[]): void;

    const RBF_MODE: number;
    const KNN_MODE: number;
    const L1_DISTANCE: number;
    const LSUP_DISTANCE: number;
    const NO_MATCH: number;
    const MIN_CONTEXT: number;
    const MAX_CONTEXT: number;
    const MAX_VECTOR_SIZE: number;
    const FIRST_NEURON_ID: number;
    const LAST_NEURON_ID: number;
    const MAX_NEURONS: number;
}

interface Neuron {
    category: number;
    context: number;
    AIF: number;
    minIF: number;
}

// PWM
interface PWM {
    open(init: PWMInit): PWMPin;
}

interface PWMInit {
    channel: number;
    period: number;
    pulseWidth: number;
    polarity: string;
}

interface PWMPin {
    setCycles(period: number, pulseWidth: number): void;
    setMilliseconds(period: number, pulseWidth: number): void;
}

// Sensors
interface Sensor {
    readonly state: SensorState;
    readonly timestamp: number;
    frequency: number;
    start(): void;
    stop(): void;
    onchange: () => void;
    onactivate: () => void;
    onerror: (error: SensorErrorEvent) => void;
}

interface SensorOptions {
    frequency: number;
}

declare enum SensorState {
    "unconnected",
    "activating",
    "activated",
    "idle",
    "errored"
}

interface SensorErrorEvent {
    error: Error;
}

interface Accelerometer extends Sensor {
    readonly x: number;
    readonly y: number;
    readonly z: number;
}

interface AccelerometerOptions extends SensorOptions  {
    controller: string;
}

interface AccelerometerConstructor {
    new(options?: AccelerometerOptions): Accelerometer;
    readonly prototype: Accelerometer;
}

declare const Accelerometer: AccelerometerConstructor

interface Gyroscope extends Sensor {
    readonly x: number;
    readonly y: number;
    readonly z: number;
}

interface GyroscopeOptions extends SensorOptions  {
    controller: string;
}

interface GyroscopeConstructor {
    new(options?: GyroscopeOptions): Gyroscope;
    readonly prototype: Gyroscope;
}

declare const Gyroscope: GyroscopeConstructor

interface AmbientLightSensor extends Sensor {
    readonly pin: number;
    readonly illuminance: number;
}

interface AmbientLightSensorOptions extends SensorOptions  {
    controller: string;
    pin: number;
}

interface AmbientLightSensorConstructor {
    new(options?: AmbientLightSensorOptions): AmbientLightSensor;
    readonly prototype: AmbientLightSensor;
}

declare const AmbientLightSensor: AmbientLightSensorConstructor

interface TemperatureSensor extends Sensor {
    readonly celsius: number;
}

interface TemperatureSensorOptions extends SensorOptions  {
    controller: string;
}

interface TemperatureSensorConstructor {
    new(options?: TemperatureSensorOptions): TemperatureSensor;
    readonly prototype: TemperatureSensor;
}

declare const TemperatureSensor: TemperatureSensorConstructor

// Timers
interface Timer {
    setInterval(callback: (...args: any[]) => void, delay: number, ...optional: any[]): number
    setTimeout(callback: (...args: any[]) => void, delay: number, ...optional: any[]): number
    clearInterval(id: number): void
    clearTimeout(id: number): void
}

interface TimerConstructor {
    new(): Timer
    readonly prototype: Timer
}

declare const Timer: TimerConstructor

// UART
declare enum UARTParity {
    "none",
    "event",
    "odd"
}

interface UART {
    init(options: UARTOptions): UARTConnection;
}

interface UARTOptions {
    port: string;
    baud?: number;
}

interface UARTConnection extends EventEmitter {
    write(data: Buffer): void;
    setReadRange(min: number, max: number): void;
}

// WebSocket
interface WebSocket {
    Server(options: object): WebSocketServer;
}

interface WebSocketServer extends EventEmitter {
}

interface WebSocketConnection extends EventEmitter {
    send(data: Buffer, mask: boolean): void;
    ping(data: Buffer, mask: boolean): void;
    pong(data: Buffer, mask: boolean): void;
}
