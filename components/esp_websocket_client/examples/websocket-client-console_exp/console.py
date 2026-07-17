#!/usr/bin/env python
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import argparse
import asyncio

import aioconsole
import websockets

cur_websocket = None
last_input = ''


async def handler(websocket):
    global cur_websocket
    global last_input
    cur_websocket = websocket
    await aioconsole.aprint('Connected!')
    while True:
        try:
            message = await websocket.recv()
            if last_input and message.startswith(last_input):
                message = message[len(last_input):]
                last_input = ''
            await aioconsole.aprint(message.decode('utf-8'), end='')
        except websockets.exceptions.ConnectionClosedError:
            return


async def websocket_loop(host: str, port: int):
    async with websockets.serve(handler, host, port):
        await asyncio.Future()


async def console_loop():
    while True:
        cmd = await aioconsole.ainput('') + '\n'
        if cur_websocket is not None:
            await cur_websocket.send(cmd.encode('utf-8'))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--host', type=str, help='Host to listen on', default='localhost'
    )
    parser.add_argument(
        '-p', '--port', type=int, help='Port to listen on', default=8080
    )
    args = parser.parse_args()
    loop = asyncio.get_event_loop()
    loop.create_task(websocket_loop(args.host, args.port))
    loop.create_task(console_loop())
    try:
        loop.run_forever()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
