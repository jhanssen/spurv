export interface ProcessStderrEvent {
    type: "stderr";
    data?: ArrayBuffer;
    end: boolean;
}
