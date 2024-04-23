export interface ProcessStdoutEvent {
    type: "stdout";
    data?: ArrayBuffer;
    end: boolean;
}
