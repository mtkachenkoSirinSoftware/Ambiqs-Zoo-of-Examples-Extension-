# kws_uart RPC proto (from NSX `usb_rpc`)

Copied from `neuralspotx/examples/usb_rpc/proto` (nanopb 0.4.9.1).
`NsxInferRequest.input` is **32000** bytes (1 s × 16 kHz int16), not the
toy 256-byte stub. Wire framing is unchanged: 4-byte LE length + nanopb
`NsxRpcMessage`.

Their `usb_rpc` image still maps INFER to 5 toy classes (`idle`/`walk`/…).
This firmware's INFER runs `KwsApp` and prints `pred=` on **SWO**. Do not
compare the toy class to `pred=go`.
