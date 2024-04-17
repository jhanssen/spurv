export interface ProcessFinishedEvent {
    type: "finished";
    statusCode: number;
    error?: string;
    stdout?: ArrayBuffer;
    stderr?: ArrayBuffer;
}
