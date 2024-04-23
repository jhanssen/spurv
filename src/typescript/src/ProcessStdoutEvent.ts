export interface ProcessStdoutEvent {
    type: "stdout";
    data?: ArrayBuffer | string;
    end: boolean;
}
