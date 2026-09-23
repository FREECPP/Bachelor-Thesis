import os
import subprocess
import sys
import termios
import tty


def get_key():
  fd = sys.stdin.fileno()
  old_settings = termios.tcgetattr(fd)
  try:
    tty.setraw(sys.stdin.fileno())
    ch = sys.stdin.read(1)
  finally:
    termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
  return ch


def stop_stream():
  subprocess.run(
      ['pkill', 'rpicam-vid'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
  )


def start_stream():
  stream_cmd = [
      'nohup',
      'rpicam-vid',
      '-t',
      '0',
      '--inline',
      '-g',
      '2',
      '--width',
      '1280',
      '--height',
      '720',
      '--framerate',
      '30',
      '--bitrate',
      '2000000',
      '-o',
      'udp://192.168.10.2:12345',
      '--shutter',
      '10000',
      '--gain',
      '2.0',
      '--autofocus-mode',
      'manual',
      '--lens-position',
      '4.0',
  ]
  with open(os.devnull, 'w') as devnull:
    subprocess.Popen(
        stream_cmd, stdout=devnull, stderr=devnull, preexec_fn=os.setsid
    )


def main():
  print('==================================================')
  print(' Smartes Kalibrierungs-Tool gestartet')
  print(' [Leertaste] = Foto machen (Stream schaltet kurz um)')
  print(' [q]         = Beenden')
  print('==================================================')

  img_count = 1
  start_stream()

  while True:
    key = get_key()

    if key == ' ':
      filename = f'calibration_photo_{img_count:02d}.jpg'
      print(f'\n[INFO] Nehme Bild auf: {filename} ...', end='', flush=True)

      stop_stream()

      still_cmd = [
          'rpicam-still',
          '-t',
          '500',
          '--width',
          '1280',
          '--height',
          '720',
          '--shutter',
          '10000',
          '--gain',
          '2.0',
          '--autofocus-mode',
          'manual',
          '--lens-position',
          '4.0',
          '-o',
          filename,
      ]

      result = subprocess.run(still_cmd, capture_output=True)

      if result.returncode == 0:
        print(f' [ERFOLG]')
        img_count += 1
      else:
        print(f' [FEHLER]')

      start_stream()
      print(
          'Stream läuft wieder. Warte auf nächste Leertaste oder [q] zum'
          ' Beenden...'
      )

    elif key.lower() == 'q':
      print('\n[INFO] Programm wird beendet.')
      stop_stream()
      break


if __name__ == '__main__':
  main()
