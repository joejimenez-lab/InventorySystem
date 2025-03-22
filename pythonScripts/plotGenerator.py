import pyodbc
import pandas as pd
import matplotlib.pyplot as plt
import os

os.makedirs('../assets', exist_ok=True)

conn_str = (

)

# Connect to the PostgreSQL database using the DSN-based connection string
conn = pyodbc.connect(conn_str)

# Query for books borrowed per day
query_day = """
SELECT DATE_TRUNC('day', borrow_date) AS borrow_date, COUNT(*) AS books_borrowed
FROM borrowed_books
GROUP BY borrow_date
ORDER BY borrow_date;
"""

# Query for books borrowed per week
query_week = """
SELECT DATE_TRUNC('week', borrow_date) AS borrow_week, COUNT(*) AS books_borrowed
FROM borrowed_books
GROUP BY borrow_week
ORDER BY borrow_week;
"""

# Query for books borrowed per month
query_month = """
SELECT DATE_TRUNC('month', borrow_date) AS borrow_month, COUNT(*) AS books_borrowed
FROM borrowed_books
GROUP BY borrow_month
ORDER BY borrow_month;
"""

# Query for books borrowed per year
query_year = """
SELECT DATE_TRUNC('year', borrow_date) AS borrow_year, COUNT(*) AS books_borrowed
FROM borrowed_books
GROUP BY borrow_year
ORDER BY borrow_year;
"""

df_day = pd.read_sql(query_day, conn)
df_week = pd.read_sql(query_week, conn)
df_month = pd.read_sql(query_month, conn)
df_year = pd.read_sql(query_year, conn)

# Plot all four time periods (day, week, month, year)
fig, axes = plt.subplots(2, 2, figsize=(14, 10))

# Plot books borrowed per day
axes[0, 0].plot(df_day['borrow_date'], df_day['books_borrowed'], marker='o', linestyle='-', color='b')
axes[0, 0].set_title('Books Borrowed per Day')
axes[0, 0].set_xlabel('Date')
axes[0, 0].set_ylabel('Books Borrowed')
axes[0, 0].tick_params(axis='x', rotation=45)
axes[0, 0].set_ylim(bottom=0)  # Set y-axis to start at 0

# Plot books borrowed per week
axes[0, 1].plot(df_week['borrow_week'], df_week['books_borrowed'], marker='o', linestyle='-', color='g')
axes[0, 1].set_title('Books Borrowed per Week')
axes[0, 1].set_xlabel('Week')
axes[0, 1].set_ylabel('Books Borrowed')
axes[0, 1].tick_params(axis='x', rotation=45)
axes[0, 1].set_ylim(bottom=0)  # Set y-axis to start at 0

# Plot books borrowed per month
axes[1, 0].plot(df_month['borrow_month'], df_month['books_borrowed'], marker='o', linestyle='-', color='r')
axes[1, 0].set_title('Books Borrowed per Month')
axes[1, 0].set_xlabel('Month')
axes[1, 0].set_ylabel('Books Borrowed')
axes[1, 0].tick_params(axis='x', rotation=45)
axes[1, 0].set_ylim(bottom=0)  # Set y-axis to start at 0

# Plot books borrowed per year
axes[1, 1].plot(df_year['borrow_year'], df_year['books_borrowed'], marker='o', linestyle='-', color='m')
axes[1, 1].set_title('Books Borrowed per Year')
axes[1, 1].set_xlabel('Year')
axes[1, 1].set_ylabel('Books Borrowed')
axes[1, 1].tick_params(axis='x', rotation=45)
axes[1, 1].set_ylim(bottom=0)  # Set y-axis to start at 0

plt.tight_layout()
plt.savefig('../assets/borrowed_books_per_day.png')
plt.close()

query_day_users = """
SELECT DATE_TRUNC('day', created_time) AS registration_date, COUNT(*) AS users_registered
FROM users
GROUP BY registration_date
ORDER BY registration_date;
"""

# Query for user registrations per week
query_week_users = """
SELECT DATE_TRUNC('week', created_time) AS registration_week, COUNT(*) AS users_registered
FROM users
GROUP BY registration_week
ORDER BY registration_week;
"""

# Query for user registrations per month
query_month_users = """
SELECT DATE_TRUNC('month', created_time) AS registration_month, COUNT(*) AS users_registered
FROM users
GROUP BY registration_month
ORDER BY registration_month;
"""

# Query for user registrations per year
query_year_users = """
SELECT DATE_TRUNC('year', created_time) AS registration_year, COUNT(*) AS users_registered
FROM users
GROUP BY registration_year
ORDER BY registration_year;
"""

df_day_users = pd.read_sql(query_day_users, conn)
df_week_users = pd.read_sql(query_week_users, conn)
df_month_users = pd.read_sql(query_month_users, conn)
df_year_users = pd.read_sql(query_year_users, conn)

# Plot all four time periods (day, week, month, year) for user registrations
fig, axes = plt.subplots(2, 2, figsize=(14, 10))

# Plot user registrations per day
axes[0, 0].plot(df_day_users['registration_date'], df_day_users['users_registered'], marker='o', linestyle='-', color='b')
axes[0, 0].set_title('User Registrations per Day')
axes[0, 0].set_xlabel('Date')
axes[0, 0].set_ylabel('Users Registered')
axes[0, 0].tick_params(axis='x', rotation=45)
axes[0, 0].set_ylim(bottom=0)  # Set y-axis to start at 0

# Plot user registrations per week
axes[0, 1].plot(df_week_users['registration_week'], df_week_users['users_registered'], marker='o', linestyle='-', color='g')
axes[0, 1].set_title('User Registrations per Week')
axes[0, 1].set_xlabel('Week')
axes[0, 1].set_ylabel('Users Registered')
axes[0, 1].tick_params(axis='x', rotation=45)
axes[0, 1].set_ylim(bottom=0)  # Set y-axis to start at 0

# Plot user registrations per month
axes[1, 0].plot(df_month_users['registration_month'], df_month_users['users_registered'], marker='o', linestyle='-', color='r')
axes[1, 0].set_title('User Registrations per Month')
axes[1, 0].set_xlabel('Month')
axes[1, 0].set_ylabel('Users Registered')
axes[1, 0].tick_params(axis='x', rotation=45)
axes[1, 0].set_ylim(bottom=0)  # Set y-axis to start at 0

# Plot user registrations per year
axes[1, 1].plot(df_year_users['registration_year'], df_year_users['users_registered'], marker='o', linestyle='-', color='m')
axes[1, 1].set_title('User Registrations per Year')
axes[1, 1].set_xlabel('Year')
axes[1, 1].set_ylabel('Users Registered')
axes[1, 1].tick_params(axis='x', rotation=45)
axes[1, 1].set_ylim(bottom=0)  # Set y-axis to start at 0

plt.tight_layout()
plt.savefig('../assets/user_registrations_per_day.png')
plt.close()

# Close the connection
conn.close()