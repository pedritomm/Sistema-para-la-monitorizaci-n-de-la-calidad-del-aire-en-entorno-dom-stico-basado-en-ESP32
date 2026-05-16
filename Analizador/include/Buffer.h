#ifndef BUFFER_H
#define BUFFER_H

#include <string>
#include <queue>
#include <mutex>

class Buffer
{
    public: 
        struct Message
        {

            std::string topic;
            std::string payload;

        };
        Buffer(uint8_t max_size = 15); // Constructor  // Por defecto el tamaño maximo será 15
        ~Buffer(); // Destructor

        bool isReceptionEmpty();
        bool isSendEmpty();
        Message getReceivedMessage();
        bool setReceivedMessage(Message msg, bool remove_oldest = true); // Por defecto te dará un true
        bool setMessageToSend(Message msg, bool remove_oldest = true);
        Message getMessageToSend();


    private:

        uint8_t m_max_size; // Limita la cantidad de datos que puede tener la cola del buffer
        std::queue<Message> m_reception;
        std::queue<Message> m_send;
        std::mutex m_reception_mutex; // Evita que dos accedan a un dato al mismo tiempo
        std::mutex m_send_mutex; // Evita que dos accedan a un dato al mismo tiempo
};

#endif //BUFFER_H