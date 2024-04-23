export interface ProcessStderrEvent {
    type: "stderr";
    data?: ArrayBuffer | string;
    end: boolean;
}
