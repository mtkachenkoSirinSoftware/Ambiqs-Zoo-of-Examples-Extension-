# kws_uart RPC proto

Copied from `neuralspotx/examples/usb_rpc/proto` (nanopb 0.4.9.1). Wire
framing is unchanged: 4-byte LE length + nanopb `NsxRpcMessage`.

`NsxInferRequest.input` is **32000** bytes (1 s × 16 kHz int16). Stock
`usb_rpc` uses 256 bytes and a five-class stub. This firmware’s INFER runs
`KwsApp` and prints `pred=` on SWO.
