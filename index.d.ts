export = AlsaCapture;

declare interface AlsaCapture {
    on(event: "audio", listener: (data: Uint8Array) => void): this;
    on(event: "close", listener: () => void): this;
    on(event: "error", listener: (error: Error) => void): this;
    on(event: "overrun", listener: () => void): this;
    on(event: "periodSizeDeviating", listener: (actualPeriodSize: number) => void): this;
    on(event: "periodTime", listener: (periodTime: number) => void): this;
    on(event: "rateDeviating", listener: (actualRate: number) => void): this;
    on(event: "readError", listener: (error: string) => void): this;
    on(event: "shortRead", listener: (framesRead: number) => void): this;

    on(event: string, listener: Function): this;
}

declare class AlsaCapture {
    constructor(options?: {
        channels?: number;
        debug?: boolean;
        format?: string;
        periodSize?: number;
        periodTime?: number;
        rate?: number;
    });

    close(): void;
}
